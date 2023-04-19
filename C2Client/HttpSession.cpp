
#include "pch.h"

#include <system_error>

#include "HttpConnection.h"
#include "WinHttpAutoHandle.h"

HttpConnection::HttpConnection(HINTERNET WinHttpHandle,
	const std::wstring& HostName) : winhttpHandle(WinHttpHandle) {

	connectionHandle = std::move(WinHttpAutoHandle(WinHttpConnect(WinHttpHandle, HostName.c_str(), INTERNET_DEFAULT_PORT, 0)));
	if (!connectionHandle.valid()) {
		// we'll figure out error handling later
	}
}

void HttpConnection::SendRequest(const std::wstring& Path, const std::wstring& Contents, const std::wstring* Headers) {
	auto requestHandle = WinHttpAutoHandle(WinHttpOpenRequest(this->connectionHandle.get(), L"POST",
		Path.c_str(), nullptr, WINHTTP_NO_REFERER, nullptr, 0));

	if (!requestHandle.valid()) {
		// error handling later
	}

}	