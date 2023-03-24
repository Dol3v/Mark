#pragma once

#include <ntifs.h>

#include "Vector.h"
#include "Mutex.h"

/*
	Handles IRP handlers hooking.
*/
class IrpHookManager {

	/*
		Data describing a hook
	*/
	struct HookState {
		LIST_ENTRY Entry;
		PDRIVER_OBJECT Driver;
		PDRIVER_DISPATCH OriginalHandler;
		ULONG MajorFunction;
		// Completion routine in case of a completion routine hook
		PIO_COMPLETION_ROUTINE CompletionRoutine = nullptr;
		PVOID Context = nullptr;
	};

public:
	IrpHookManager();

	/*
		Hooks an IRP handler of a driver.
	*/
	NTSTATUS HookIrpHandler(const PUNICODE_STRING DriverName, ULONG MajorFunction, PDRIVER_DISPATCH NewIrpHandler, PIO_COMPLETION_ROUTINE CompletionRoutine = nullptr, PVOID CompletionContext = nullptr);

	/*
		Installs a completion routine on any IOCTL IRP that reaches the driver.
	*/
	NTSTATUS InstallCompletionRoutine(const PUNICODE_STRING DriverName, PIO_COMPLETION_ROUTINE CompletionRoutine, PVOID CompletionContext);

	/*
		Restores the original handler for an IRP handler.
	*/
	VOID RestoreOriginal(const PUNICODE_STRING DriverName, ULONG MajorFunction);

	~IrpHookManager();

	/*
		IOCTL IRP handler that sets the completion routine of the IRP based off previously set hooks.
	*/
	friend NTSTATUS SetCompletionRoutineIoctlHook(PDEVICE_OBJECT DeviceObject, PIRP Irp);

private:
	LIST_ENTRY hooksHead;
	Mutex mutex;
};
