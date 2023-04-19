#pragma once

#include "KernelPrimitives.h"

/*
	PE related utils
*/

namespace PeUtils {

	/*
		Returns the addresses of win32kbase.sys and ntoskrnl
	*/
	std::tuple<ULONG64, ULONG64> GetKernelModuleAddresses();

	/*
		Returns the offset of an exported function from an in-memory image
	*/
	PVOID GetExportAddress(PBYTE ImageBase, const char* ExportName);

	DWORD GetImageSize(PVOID ImageBase);
	
}

