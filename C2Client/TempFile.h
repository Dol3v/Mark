#pragma once

#include "pch.h"
#include "../DriverLoader/AutoHandle.h"

#include <stdexcept>

/*
	Wrapper for a temp file
*/
class TempFile {
public:
	TempFile(DWORD DesiredAccess = GENERIC_READ | GENERIC_WRITE) : handle(0) {
		char tempPath[MAX_PATH];

		DWORD ret = 0;
		if ((ret = GetTempPathA(MAX_PATH, tempPath)) == 0 || ret > MAX_PATH) {
			throw std::runtime_error("Failed to get temp path");
		}

		char tempFileName[MAX_PATH];
		if (GetTempFileNameA(tempPath, "DATA", 0, tempFileName) == 0) {
			throw std::runtime_error("Failed to get temp file name");
		}
		path = std::string(tempFileName);

		auto fileHandle = CreateFileA(tempFileName, DesiredAccess, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		handle = Utils::AutoHandle(fileHandle);
	}

	HANDLE GetHandle() {
		return handle.get();
	}

	const std::string& GetPath() {
		return this->path;
	}

private:
	Utils::AutoHandle handle;
	std::string path;
};
