#pragma once

#include "RemoveProtections.h"
#include "IrpHook.h"

/*
	General global definitions and macros
*/


/*
	Globals used in the project
*/
struct Globals {
	// Apc rundown protection
	EX_RUNDOWN_REF ApcRundownProtection = { 0 };

	// Global data relating to networking UM process
	struct _NetworkingProcess {
		Ppl::ProcessProtectionPolicy PrevProtectionLevel = { 0 };
		HANDLE Pid = 0;
		bool FoundFirstThread = false;
	} Network;

	// Cached offsets/addresses of undocumented structues
	// and functions
	struct _CachedOffsets {
		ULONG PsProtectionOffset = 0;
		POBJECT_TYPE IoDriverObjectType = nullptr;
	} CachedOffsets;

	IrpHookManager* HookManager;
};

extern Globals g_Globals;

/*
	Function declerations
*/

void DriverUnload(PDRIVER_OBJECT);

NTSTATUS IoctlDispatch(PDEVICE_OBJECT, PIRP);

NTSTATUS HandleCopyToMemory(PIRP, PIO_STACK_LOCATION);

NTSTATUS HandleRestoreProtection(PIRP);

NTSTATUS DefaultDispatch(PDEVICE_OBJECT, PIRP);

NTSTATUS CreateClose(PDEVICE_OBJECT, PIRP);

NTSTATUS CompleteIrp(PIRP, NTSTATUS, ULONG_PTR);

void OnCreateProcess(HANDLE, HANDLE, BOOLEAN);

void OnCreateThread(HANDLE, HANDLE, BOOLEAN);

void InjectCode(PVOID, PVOID, PVOID);

NTSTATUS CopyToReadOnlyBuffer(PVOID, const void*, SIZE_T);
