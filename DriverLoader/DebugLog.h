#pragma once

#include <sstream>
#include <Windows.h>

class logstream {
public:
	logstream(const char* prefix) : prefix(prefix) {
	}

	template <typename... Types>
	void info(Types... types) {
		logToDebug("[INFO] ", types..., "\n");
	}

	template <typename... Types>
	void debug(Types... types) {
		logToDebug("[DEBUG] ", types..., "\n");
	}

	template <typename... Types>
	void error(Types... types) {
		logToDebug("[ERROR] ", types..., " ec=0x", std::hex, GetLastError(), "\n");
	}

private:

	template <typename First, typename... Args>
	void logToStream(std::stringstream& stream, First first, Args... rest) {
		stream << first;
		logToStream(stream, rest...);
	}

	template <typename T>
	void logToStream(std::stringstream& stream, T t) {
		stream << t;
	}

	template <typename... Types>
	void logToDebug(Types... types) {
		std::stringstream s;
		logToStream(s, prefix, types...);
		OutputDebugStringA(s.str().c_str());
	}

private:
	const char* prefix;
};

extern logstream LOGGER;
