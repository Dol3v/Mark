#include "IrpHook.h"
#include "MarkDriver.h"
#include "AutoLock.h"
#include "Utils.h"

IrpHookManager::IrpHookManager() {
	::InitializeListHead(&this->hooksHead);
	this->mutex.Init();
}

NTSTATUS IrpHookManager::HookIrpHandler(const PUNICODE_STRING DriverName, ULONG MajorFunction, PDRIVER_DISPATCH NewIrpHandler, 
	PIO_COMPLETION_ROUTINE CompletionRoutine, PVOID CompletionContext) {
	if (MajorFunction >= IRP_MJ_MAXIMUM_FUNCTION) {
		LOG_FAIL_VA("Invalid major function %ul", MajorFunction);
		return STATUS_INVALID_PARAMETER;
	}

	// til we do ref counting
	auto* hookState = reinterpret_cast<HookState*>(::ExAllocatePoolWithTag(PagedPool, sizeof(HookState), DRIVER_TAG));
	if (hookState == nullptr) {
		LOG_FAIL("Allocation of hook state failed");
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	// get driver object's address
	PDRIVER_OBJECT driverObject = nullptr;
	auto status = ObReferenceObjectByName(DriverName,
		0, nullptr, GENERIC_ALL, g_Globals.CachedOffsets.IoDriverObjectType, KernelMode,
		nullptr, reinterpret_cast<PVOID*>(&driverObject));
	PRINT_AND_RETURN_IF_FAILED(status, "Failed to retrieve object by name");
	
	// do da switch
	auto& currentMajorFunctionHandler = driverObject->MajorFunction[MajorFunction];
	InterlockedExchangePointer(reinterpret_cast<PVOID*>(&currentMajorFunctionHandler), NewIrpHandler);

	// insert entry into entry
	hookState->Driver = driverObject;
	hookState->OriginalHandler = currentMajorFunctionHandler;
	hookState->MajorFunction = MajorFunction;
	hookState->CompletionRoutine = CompletionRoutine;
	hookState->Context = CompletionContext;
	::InsertHeadList(&this->hooksHead, &hookState->Entry);

	// cleanup
	::ObDereferenceObject(driverObject);
	return STATUS_SUCCESS;
}

NTSTATUS SetCompletionRoutineIoctlHook(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
	const auto* driverObject = DeviceObject->DriverObject;
	auto& listHead = g_Globals.HookManager->hooksHead;
	const auto* entry = listHead.Flink;
	const IrpHookManager::HookState* matchingHookState = nullptr;

	{
		AutoLock<Mutex> lock();
		while (entry != &listHead) {
			// find matching installed hook
			const auto* hook = CONTAINING_RECORD(entry, IrpHookManager::HookState, Entry);
			if (hook->Driver == driverObject && hook->MajorFunction == IRP_MJ_DEVICE_CONTROL) {
				// found hook
				if (hook->CompletionRoutine != nullptr) {
					::IoSetCompletionRoutine(Irp, hook->CompletionRoutine, hook->Context, TRUE, FALSE, FALSE);
				}
				matchingHookState = hook;
				break;
			}
		}
	}
	

	if (matchingHookState == nullptr) {
		LOG_FAIL_VA("Couldn't find matching hook, device object=0x%p", DeviceObject);
		return STATUS_INTERNAL_ERROR;
	}

	// call original handler
	return matchingHookState->OriginalHandler(DeviceObject, Irp);
}

NTSTATUS IrpHookManager::InstallCompletionRoutine(const PUNICODE_STRING DriverName, PIO_COMPLETION_ROUTINE CompletionRoutine, PVOID CompletionContext) {
	return this->HookIrpHandler(DriverName, IRP_MJ_DEVICE_CONTROL, SetCompletionRoutineIoctlHook, CompletionRoutine, CompletionContext);
}


VOID IrpHookManager::RestoreOriginal(const PUNICODE_STRING DriverName, ULONG MajorFunction) {
	// get driver object's address
	PDRIVER_OBJECT driverObject = nullptr;
	auto status = ObReferenceObjectByName(DriverName,
		0, nullptr, GENERIC_ALL, g_Globals.CachedOffsets.IoDriverObjectType, KernelMode,
		nullptr, reinterpret_cast<PVOID*>(&driverObject));
	if (!NT_SUCCESS(status)) {
		LOG_FAIL_VA("Failed to retrieve driver object %ws by name, status=0x%08x", DriverName->Buffer, status);
		return;
	}

	AutoLock<Mutex> lock(mutex);
	auto* entry = this->hooksHead.Flink;
	while (entry != &this->hooksHead) {
		auto* hook = CONTAINING_RECORD(entry, IrpHookManager::HookState, Entry);
		if (hook->Driver == driverObject && hook->MajorFunction == MajorFunction) {
			// restore original function and unhook stuff
			::InterlockedExchangePointer(reinterpret_cast<PVOID*>(&driverObject->MajorFunction[MajorFunction]),
				hook->OriginalHandler);
			::RemoveEntryList(&hook->Entry);
			::ExFreePoolWithTag(hook, DRIVER_TAG);
			LOG_SUCCESS_VA("Removed hook %p from list", hook);
			break;
		}
	}
}

IrpHookManager::~IrpHookManager() {
	AutoLock<Mutex> lock(mutex);
	while (!IsListEmpty(&this->hooksHead)) {
		auto* entry = RemoveHeadList(&this->hooksHead);
		 
		// unhook and free
		auto* hook = CONTAINING_RECORD(entry, HookState, Entry);
		InterlockedExchangePointer(reinterpret_cast<PVOID*>(&hook->Driver[hook->MajorFunction]),
			hook->OriginalHandler);
		::ExFreePoolWithTag(hook, DRIVER_TAG);
	}
}
