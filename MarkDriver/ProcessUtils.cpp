#include "ProcessUtils.h"
#include "AutoHandle.h"
#include "Macros.h"

PEPROCESS GetProcessByName(const CHAR* ImageName) {
	CHAR* process = reinterpret_cast<CHAR*>(PsInitialSystemProcess);
	do {
		KdPrint(("[*] Scanning process %s\n", process + IMAGE_FILE_NAME_OFFSET));
		if (!::strcmp(process + IMAGE_FILE_NAME_OFFSET, ImageName)) {
			return reinterpret_cast<PEPROCESS>(process);
		}
		process = reinterpret_cast<CHAR*>(reinterpret_cast<LIST_ENTRY*>(process + ACTIVE_PROCESS_LINKS_OFFSET)->Flink) - ACTIVE_PROCESS_LINKS_OFFSET;
	} while (process != reinterpret_cast<CHAR*>(PsInitialSystemProcess));
	return nullptr;
}

NTSTATUS KillProcess(PEPROCESS TargetProcess) {
	__try {
		AutoHandle handle(TargetProcess, *PsProcessType);
		auto status = ::ZwTerminateProcess(handle.get(), STATUS_SUCCESS);
		PRINT_AND_RETURN_IF_FAILED(status, "Failed to terminate process");
		LOG_SUCCESS("Closed process successfully");
		return STATUS_SUCCESS;

	} __except (EXCEPTION_EXECUTE_HANDLER) {
		auto status = ::GetExceptionCode();
		LOG_FAIL_WITH_STATUS("Exception occured in " STRINGIFY(KillProcess), status);
		return status;
	}
}
