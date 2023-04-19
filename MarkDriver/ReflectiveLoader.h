#pragma once

#include <ntifs.h>

/*
	Function declerations related to the reflective loading process
*/


namespace Loader {


	// function types

	typedef PVOID(NTAPI* ExAllocatePoolWithTag_t)(_In_ POOL_TYPE PoolType, _In_ SIZE_T NumberOfBytes, _In_ ULONG Tag);
	typedef VOID(NTAPI* ExFreePoolWithTag_t)(_In_ PVOID P, _In_ ULONG Tag);
	typedef NTSTATUS(NTAPI* IoCreateDriver_t)(_In_ PUNICODE_STRING DriverName, _In_opt_ PDRIVER_INITIALIZE InitializationFunction);
	typedef PVOID(NTAPI* MmGetSystemRoutineAddress_t)(_In_ PUNICODE_STRING SystemRoutineName);
	typedef PIMAGE_NT_HEADERS(NTAPI* RtlImageNtHeader_t)(_In_ PVOID ModuleAddress);
	typedef PVOID(NTAPI* RtlImageDirectoryEntryToData_t)(_In_ PVOID Base, IN BOOLEAN MappedAsImage, _In_ USHORT DirectoryEntry, _Out_ PULONG Size);
	typedef NTSTATUS(NTAPI* RtlQueryModuleInformation_t)(ULONG* InformationLength, ULONG SizePerModule, PVOID InformationBuffer);

	struct Functions {
		ExAllocatePoolWithTag_t ExAllocatePoolWithTag;
		ExFreePoolWithTag_t ExFreePoolWithTag;
		IoCreateDriver_t IoCreateDriver;
		MmGetSystemRoutineAddress_t MmGetSystemRoutineAddress;
		RtlImageNtHeader_t RtlImageNtHeader;
		RtlImageDirectoryEntryToData_t RtlImageDirectoryEntryToData;
		RtlQueryModuleInformation_t RtlQueryModuleInformation;
	};

	struct LoaderContext {
		MmGetSystemRoutineAddress_t MmGetSystemRoutineAddress;
		PUCHAR ImageBase;
	};

	#pragma comment(linker, "/export:?RelectiveLoader@Loader@@YAXPEAULoaderContext@1@@Z ")

	/*
		Loads the driver from its in-memory image.
	*/
	VOID RelectiveLoader(LoaderContext* Context);

	NTSTATUS ApplyRelocations(Functions* Funcs, PUCHAR ImageBase);

	NTSTATUS LoadImports(Functions* Funcs, PUCHAR ImageBase);

	PVOID GetFunctionByName(Functions* Funcs, PUCHAR ImageBase, const CHAR* FunctionName);

	PVOID GetModuleByName(Functions* Funcs, const CHAR* ModuleName);
	
	typedef struct _RTL_MODULE_EXTENDED_INFO
	{
		PVOID ImageBase;
		ULONG ImageSize;
		USHORT FileNameOffset;
		CHAR FullPathName[0x100];
	} RTL_MODULE_EXTENDED_INFO, * PRTL_MODULE_EXTENDED_INFO;
}


#define DECLARE_UNICODE_STRING(_var, _string) \
	WCHAR _var ## _buffer[] = _string; \
	__pragma(warning(push)) \
	__pragma(warning(disable:4221)) __pragma(warning(disable:4204)) \
	UNICODE_STRING _var = { sizeof(_string)-sizeof(WCHAR), sizeof(_string), (PWCH)_var ## _buffer } \
	__pragma(warning(pop))
