#pragma once

#define WIN32_MEAN_AND_LEAN

#include <Windows.h>
#include <winternl.h>

/*
	Shellcode that loads the driver.

	Big thanks to the reflective dll loading article on DW, and to
	https://github.com/Professor-plum/Reflective-Driver-Loader.

	Why 2? Cause DriverLoaderShellcode gave me linker errors and probably
	reduced 5 years from my life span.
*/

/*
	Returns the size of the loader shellcode.
*/
size_t GetLoaderShellcodeSize();

/*
	Runs the KM shellcode that loads the driver in a new thread.
*/
VOID LoadDriverShellcode(PVOID MmGetSystemRoutineAddress, PVOID KernelImageBase, ULONG32 reflectiveLoaderRva);

typedef PVOID(NTAPI* uMmGetSystemRoutineAddress_t)(_In_ PUNICODE_STRING SystemRoutineName);
typedef PVOID(NTAPI* uExAllocatePoolWithTag_t)(_In_ ULONG PoolType, _In_ SIZE_T NumberOfBytes, _In_ ULONG Tag);
typedef NTSTATUS(NTAPI* uPsCreateSystemThread_t)(_Out_ PHANDLE ThreadHandle, _In_ ULONG DesiredAccess, _In_opt_ POBJECT_ATTRIBUTES ObjectAttributes, _In_opt_ HANDLE ProcessHandle, _Out_opt_ LPVOID ClientId, _In_ LPVOID StartRoutine, _In_opt_ PVOID StartContext);


struct LoaderContext {
	uMmGetSystemRoutineAddress_t MmGetSystemRoutine;
	PVOID ImageBase;
};

#define DECLARE_UNICODE_STRING(_var, _string) \
	WCHAR _var ## _buffer[] = _string; \
	__pragma(warning(push)) \
	__pragma(warning(disable:4221)) __pragma(warning(disable:4204)) \
	UNICODE_STRING _var = { sizeof(_string)-sizeof(WCHAR), sizeof(_string), (PWCH)_var ## _buffer } \
	__pragma(warning(pop))