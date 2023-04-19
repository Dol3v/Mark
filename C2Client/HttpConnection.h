#pragma once

#include <Windows.h>
#include <winhttp.h>

#include <string>

#include "WinHttpAutoHandle.h"

/*
	Http connection
*/
class HttpConnection {

public:
	HttpConnection(HINTERNET WinHttpHandle, const std::wstring& HostName);

	void SendRequest(const std::wstring& Path, const std::wstring& Contents, const std::wstring* Headers = nullptr);

private:
	WinHttpAutoHandle winhttpHandle;
	WinHttpAutoHandle connectionHandle;
};