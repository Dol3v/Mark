
#include <ntifs.h>

#include "MarkDriver.h"
#include "Memory.h"
#include "Utils.h"
#include "RemoveProtections.h"
#include "ProcessUtils.h"
#include "Vector.h"
#include "ModuleFinder.h"
#include "PushLock.h"
#include "AutoLock.h"
#include "Macros.h"


namespace Protections {

	// Protection Remover

	struct UnprotectedProcessEntry {
		LIST_ENTRY Entry;
		Ppl::ProcessProtectionPolicy PrevProtection;
		ULONG Pid;
		PEPROCESS Process;
	};

	ProtectionRemover::ProtectionRemover() : listMutex() {
		InitializeListHead(&unprotectedProcesses);
		listMutex.Init();
	}

	NTSTATUS ProtectionRemover::RemoveProcessProtection(ULONG Pid) {
		PEPROCESS process = nullptr;
		auto status = PsLookupProcessByProcessId(UlongToHandle(Pid), &process);
		PRINT_AND_RETURN_IF_FAILED(status, "Failed to find process by id");

		Ppl::ProcessProtectionPolicy prevPolicy = { 0 };
		__try {
			status = Protections::RemovePsProtection(process, prevPolicy);
		}
		__except (EXCEPTION_CONTINUE_EXECUTION) {
			status = GetExceptionCode();
			LOG_FAIL_WITH_STATUS("Exception raised on " STRINGIFY(Protections::RemovePsProtection), status);
		}
		PRINT_AND_RETURN_IF_FAILED(status, "Failed to remove protections");

		auto* newEntry = new (POOL_FLAG_PAGED) UnprotectedProcessEntry{};
		newEntry->PrevProtection = prevPolicy;
		newEntry->Pid = Pid;
		newEntry->Process = process;
		{
			AutoLock<Mutex> lock(listMutex);
			InsertHeadList(&unprotectedProcesses, &newEntry->Entry);
		}
		LOG_SUCCESS("Successfully removed protection from process");
		return STATUS_SUCCESS;
	}

	NTSTATUS ProtectionRemover::RestoreProcessProtection(ULONG Pid) {
		AutoLock<Mutex> lock(listMutex);
		auto* currentEntry = (&unprotectedProcesses)->Flink;
		while (currentEntry != &unprotectedProcesses) {
			auto* item = CONTAINING_RECORD(currentEntry, UnprotectedProcessEntry, Entry);
			if (item->Pid == Pid) {
				__try {
					Ppl::ModifyProcessProtectionPolicy(item->Process, item->PrevProtection);
				}
				__except (EXCEPTION_CONTINUE_EXECUTION) {
					auto status = GetExceptionCode();
					LOG_FAIL_WITH_STATUS(STRINGIFY(Ppl::ModifyProcessProtectionPolicy) " raised exception", status);
					return status;
				}
				RemoveEntryList(currentEntry);
				ExFreePoolWithTag(item, DRIVER_TAG);
				return STATUS_SUCCESS;
			}
		}
		return STATUS_NOT_FOUND;
	}

	ProtectionRemover::~ProtectionRemover() {
		AutoLock<Mutex> lock(listMutex);
		while (!IsListEmpty(&unprotectedProcesses)) {
			auto* entry = RemoveHeadList(&unprotectedProcesses);
			auto* item = CONTAINING_RECORD(entry, UnprotectedProcessEntry, Entry);

			Ppl::ModifyProcessProtectionPolicy(item->Process, item->PrevProtection);
			ExFreePoolWithTag(item, DRIVER_TAG);
		}
	}

	// Top level functions

	NTSTATUS GetProtectingCallbacks(Vector<CallbackEntry*>& RelevantCallbacks, const CHAR* DriverImageName);

	NTSTATUS RemovePsProtection(PEPROCESS TargetProcess, Ppl::ProcessProtectionPolicy& PrevProtection) {
		__try {
			Ppl::GetProcessProtection(TargetProcess, &PrevProtection);

			// turn off protection
			Ppl::ProcessProtectionPolicy newProtection = { 0 };
			Ppl::ModifyProcessProtectionPolicy(TargetProcess, newProtection);
			return STATUS_SUCCESS;
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {
			auto status = ::GetExceptionCode();
			LOG_FAIL_WITH_STATUS(STRINGIFY(Protections::RemovePsProtection) " Failed", status);
			return status;
		}
	}

	NTSTATUS RemoveProtectingObjectCallbacks(const CHAR* DriverImageName) {
		__try {
			Vector<CallbackEntry*> relevantCallbacks;
			auto status = GetProtectingCallbacks(relevantCallbacks, DriverImageName);
			RETURN_IF_FAILED(status);
			auto* typeLock = reinterpret_cast<PEX_PUSH_LOCK>(reinterpret_cast<UCHAR*>(*PsProcessType) + OBJECT_TYPE_LOCK_OFFSET);
			{
				// modify callback list
				
				PushLock pushLock = { typeLock, true };
				AutoLock<PushLock> autoLock(pushLock);

				for (auto callback : relevantCallbacks) {
					::RemoveEntryList(&callback->Entry);
				}
			}
			return STATUS_SUCCESS;
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {
			auto status = ::GetExceptionCode();
			LOG_FAIL_WITH_STATUS(STRINGIFY(Protections::RemoveProtectingObjectCallbacks) " Failed", status);
			return status;
		}
	}

	/*
		Returns all callbacks that are registered in a specific driver.
	*/
	NTSTATUS GetProtectingCallbacks(Vector<CallbackEntry*>& RelevantCallbacks, const CHAR* DriverImageName) {
		auto* processObjectType = *PsProcessType;
		auto** callbackList = reinterpret_cast<CallbackEntry**>(reinterpret_cast<UCHAR*>(processObjectType) + OBJECT_CALLBACK_LIST_OFFSET);
		__try {
			auto* finder = g_Globals.ModuleFinder;
			finder->Refresh();

			for (auto* callback = *callbackList;
				callback != reinterpret_cast<const CallbackEntry*>(callbackList);
				callback = CONTAINING_RECORD(callback->Entry.Flink, CallbackEntry, Entry))
			{
				if (!callback->IsEnabled) {
					continue;
				}
				const auto* containingModule = finder->FindModuleByAddress(reinterpret_cast<PVOID>(callback->PreOperationFunction));
				if (containingModule == nullptr) {
					LOG_FAIL_VA("Found callback %llx, unknown module, refreshing module list and trying again", callback->PreOperationFunction);

					finder->Refresh();
					containingModule = finder->FindModuleByAddress(reinterpret_cast<PVOID>(callback->PreOperationFunction));
					if (containingModule == nullptr) {
						LOG_FAIL("Module not found, continuing...");
						continue;
					}
				}

				LOG_SUCCESS_VA("Found callback %llx in module %s", callback->PreOperationFunction, containingModule->FullPathName);
				// insert relevant callbacks
				if (!::strcmp(reinterpret_cast<const CHAR*>(containingModule->FullPathName) + containingModule->FileNameOffset, DriverImageName)) {
					RelevantCallbacks.Insert(callback);
				}
			}
			return STATUS_SUCCESS;
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {
			auto status = ::GetExceptionCode();
			LOG_FAIL_WITH_STATUS(STRINGIFY(Protections::GetProtectingCallbacks) " Failed", status);
			return status;
		}
	}
}
