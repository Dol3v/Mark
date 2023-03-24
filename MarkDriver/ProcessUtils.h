#pragma once

#include <ntifs.h>

/*
	Returns the EPROCESS of a process with a specific image name.
*/
PEPROCESS GetProcessByName(const CHAR* ImageName);

/*
	Terminates a process.
*/
NTSTATUS KillProcess(PEPROCESS TargetProcess);

// offsets in EPROCESS

constexpr size_t IMAGE_FILE_NAME_OFFSET = 0x5a8;
constexpr size_t ACTIVE_PROCESS_LINKS_OFFSET = 0x448;
