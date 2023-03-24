#include "MarkDriver.h"
#include "Macros.h"
#include "Apc.h"

NTSTATUS QueueApc(PKNORMAL_ROUTINE ApcRoutine, KPROCESSOR_MODE ApcProcessorMode, PETHREAD TargetThread, PVOID SystemArgument1, PVOID SystemArgument2) {
	auto* kapc = static_cast<KAPC*>(::ExAllocatePoolWithTag(NonPagedPool, sizeof(KAPC), DRIVER_TAG));
	if (kapc == nullptr) {
		LOG_FAIL("Failed to allocate buffer for APC");
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	KeInitializeApc(kapc,
		TargetThread,
		OriginalApcEnvironment,
		[](PKAPC apc, PKNORMAL_ROUTINE*, PVOID*, PVOID*, PVOID*) {::ExFreePoolWithTag(apc, DRIVER_TAG); },
		[](const PKAPC apc)	                                                                               // Rundown APC
		{
			::ExFreePoolWithTag(apc, DRIVER_TAG);
			::ExReleaseRundownProtection(&g_Globals.ApcRundownProtection);
		},
		ApcRoutine,
		ApcProcessorMode,
		nullptr
	);
	if (!KeInsertQueueApc(kapc, SystemArgument1, SystemArgument2, IO_NO_INCREMENT)) {
		LOG_FAIL("Failed to insert APC into queue");
		::ExFreePoolWithTag(kapc, DRIVER_TAG);
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	return STATUS_SUCCESS;
}
