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

		AutoHandle() : handle(INVALID_HANDLE_VALUE) {}

		AutoHandle(HANDLE handle) : handle(handle) {}

		// disable copying
		AutoHandle(const AutoHandle&) = delete;
		AutoHandle& operator=(const AutoHandle&) = delete;
		AutoHandle(const AutoHandle&&) = delete;
		AutoHandle& operator=(const AutoHandle&&) = delete;

		void set(HANDLE newValue) {
			if (this->handle == INVALID_HANDLE_VALUE)
				this->handle = newValue;
		}

		HANDLE get() const { return this->handle; }

		~AutoHandle() noexcept {
			::CloseHandle(this->handle);
		}
	};
}