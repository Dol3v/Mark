#pragma once

#include <ntifs.h>

#include "Ppl.h"

namespace Protections {

	/*
		Remove Ppl protections from a process; additionally returns the previous protection of the process.
	*/
	NTSTATUS RemovePsProtection(PEPROCESS TargetProcess, Ppl::ProcessProtectionPolicy& PrevProtection);

	/*
		Removes driver pre-operation process object callbacks.
	*/
	NTSTATUS RemoveProtectingObjectCallbacks(const CHAR* DriverImageName);

	/*
		Reverse-engineered object callback entry.
	*/
	#pragma pack(push, 1)
	struct CallbackEntry {
		LIST_ENTRY Entry; // 0x0
		UCHAR Pad1[0x4]; // 0x10
		UINT32 IsEnabled; // 0x14
		UCHAR Pad2[0x10]; /// 0x18
		DWORD64 PreOperationFunction;	// 0x28
	};
	#pragma pack(pop)

	constexpr size_t OBJECT_TYPE_LOCK_OFFSET = 0xb8;
	constexpr size_t OBJECT_CALLBACK_LIST_OFFSET = 0xc8;
}