#pragma once

#include <Windows.h>

namespace Utils {

	/*
		RAII Wrapper for handles
	*/
	class AutoHandle {
	private:
		HANDLE handle;
	public:
		AutoHandle(HANDLE handle) : handle(handle) {}

		// disable copying
		AutoHandle(const AutoHandle&) = delete;
		AutoHandle& operator=(const AutoHandle&) = delete;

		AutoHandle(const AutoHandle&& Other) noexcept {
			this->handle = Other.handle;
		}

		AutoHandle& operator=(const AutoHandle&& Other) noexcept {
			this->handle = Other.handle;
			return *this;
		}

		HANDLE get() const { return this->handle; }

		~AutoHandle() noexcept {
			::CloseHandle(this->handle);
		}
	};
}