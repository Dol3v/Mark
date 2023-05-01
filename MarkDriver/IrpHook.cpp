#include "IrpHook.h"
#include "MarkDriver.h"
#include "AutoLock.h"
#include "Utils.h"

ULONG MANAGER_INSTANCES = 0;

IrpHookManager::IrpHookManager() {
	if (MANAGER_INSTANCES != 0) {
		LOG_FAIL("Tried to create more than one hook manager");
		ExRaiseStatus(STATUS_ALREADY_INITIALIZED);
	}
	InitializeListHead(&this->hooksHead);
	this->mutex.Init();
	MANAGER_INSTANCES = 1;
}

NTSTATUS IrpHookManager::HookIrpHandler(const PUNICODE_STRING DriverName, ULONG MajorFunction, PDRIVER_DISPATCH NewIrpHandler, 
	PIO_COMPLETION_ROUTINE CompletionRoutine, PVOID CompletionContext) {

	if (MajorFunction >= IRP_MJ_MAXIMUM_FUNCTION) {
		LOG_FAIL_VA("Invalid major function %ul", MajorFunction);
		return STATUS_INVALID_PARAMETER;
	}

	// get driver object's address
	PDRIVER_OBJECT driverObject = nullptr;
	auto status = ObReferenceObjectByName(DriverName,
		0, nullptr, GENERIC_ALL, g_Globals.CachedOffsets.IoDriverObjectType, KernelMode,
		nullptr, reinterpret_cast<PVOID*>(&driverObject));
	PRINT_AND_RETURN_IF_FAILED(status, "Failed to retrieve object by name");

	auto& currentMajorFunctionHandler = driverObject->MajorFunction[MajorFunction];

	// til we do ref counting
	auto* hookState = reinterpret_cast<HookState*>(::ExAllocatePoolWithTag(PagedPool, sizeof(HookState), DRIVER_TAG));
	if (hookState == nullptr) {
		LOG_FAIL("Allocation of hook state failed");
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	// insert entry into entry
	hookState->Driver = driverObject;
	hookState->OriginalHandler = currentMajorFunctionHandler;
	hookState->MajorFunction = MajorFunction;
	hookState->CompletionRoutine = CompletionRoutine;
	hookState->Context = CompletionContext;

	{
		AutoLock<Mutex> lock(this->mutex);
		InsertHeadList(&this->hooksHead, &hookState->Entry);
	}

	// do da switch
	InterlockedExchangePointer(reinterpret_cast<PVOID*>(&currentMajorFunctionHandler), NewIrpHandler);

	// cleanup
	ObDereferenceObject(driverObject);
	return STATUS_SUCCESS;
}

NTSTATUS SetCompletionRoutineIoctlHook(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
	const auto* driverObject = DeviceObject->DriverObject;
	auto& listHead = g_Globals.HookManager->hooksHead;
	const auto* entry = listHead.Flink;
	const IrpHookManager::HookState* matchingHookState = nullptr;

	{
		AutoLock<Mutex> lock(g_Globals.HookManager->mutex);
		while (entry != &listHead) {
			// find matching installed hook
			const auto* hook = CONTAINING_RECORD(entry, IrpHookManager::HookState, Entry);
			if (hook->Driver == driverObject && hook->MajorFunction == IRP_MJ_DEVICE_CONTROL) {
				// found hook
				if (hook->CompletionRoutine != nullptr) {
					IoSetCompletionRoutine(Irp, hook->CompletionRoutine, hook->Context, TRUE, FALSE, FALSE);
				}
				matchingHookState = hook;
				break;
			}
			entry = entry->Flink;
		}
	}
	

	if (matchingHookState == nullptr) {
		LOG_FAIL_VA("Couldn't find matching hook, device object=0x%p", DeviceObject);
		return STATUS_INTERNAL_ERROR;
	}

	// call original handler
	return matchingHookState->OriginalHandler(DeviceObject, Irp);
}

NTSTATUS IrpHookManager::InstallCompletionRoutine(const PUNICODE_STRING DriverName, PIO_COMPLETION_ROUTINE CompletionRoutine, PVOID CompletionContext, ULONG MajorFunction) {
	return this->HookIrpHandler(DriverName, MajorFunction, SetCompletionRoutineIoctlHook, CompletionRoutine, CompletionContext);
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
		entry = entry->Flink;
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
