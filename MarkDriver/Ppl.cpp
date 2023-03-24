#include <ntifs.h>

#include "MarkDriver.h"
#include "Macros.h"
#include "Ppl.h"

namespace Ppl {


	/*
		Returns the offset of the Protection field of EPROCESS.

		Raises STATUS_NOT_FOUND if PsIsProtectedProcess was not found, and STATUS_INTERNAL_ERROR if the byte signature is no longer valid.
	*/
	UINT32 GetPsProtectionOffset() {
		PAGED_CODE();

		if (g_Globals.CachedOffsets.PsProtectionOffset != 0) {
			return g_Globals.CachedOffsets.PsProtectionOffset;
		}

		UNICODE_STRING functionName = RTL_CONSTANT_STRING(L"PsIsProtectedProcess");
		auto psIsProtectedProcessAddr = static_cast<UCHAR*>(MmGetSystemRoutineAddress(&functionName));
		if (!psIsProtectedProcessAddr) {
			LOG_FAIL("Couldn't find the address of PsIsProtectedProcess");
			::ExRaiseStatus(STATUS_NOT_FOUND);
		}

		// scan PsIsProtectedProcess for offset
		// .text:00000001402ECF60                      PsIsProtectedProcess proc near; 
		// .text:00000001402ECF60 F6 81 7A 08 00 00 07                 test    byte ptr[rcx + 87Ah], 7
		// .text:00000001402ECF67 B8 00 00 00 00					   mov     eax, 0
		// .text:00000001402ECF6C 0F 97 C0                             setnbe  al
		// .text:00000001402ECF6F C3                                   retn
		// .text:00000001402ECF6F                      PsIsProtectedProcess endp

		if (*psIsProtectedProcessAddr != 0xf6 || psIsProtectedProcessAddr[1] != 0x81) {
			LOG_FAIL("PsIsProtectedProcess doesn't use test anymore:( Change function/signature.");
			::ExRaiseStatus(STATUS_INTERNAL_ERROR);
		}
		auto offset = *reinterpret_cast<UINT32*>(psIsProtectedProcessAddr + 2);
		LOG_SUCCESS_VA("Found Protection offset in EPROCESS to be 0x%x", offset);
		g_Globals.CachedOffsets.PsProtectionOffset = offset;
		return offset;
	}

	UINT32 GetProtectionPolicyOffset() {
		return GetPsProtectionOffset() - FIELD_OFFSET(ProcessProtectionPolicy, Protection);
	}

	VOID GetProcessProtection(_In_ const PEPROCESS Process, _Inout_ ProcessProtectionPolicy* ProtectionPolicy) {
		ULONG offset = GetProtectionPolicyOffset();
		::RtlCopyMemory(ProtectionPolicy, (UCHAR*)Process + offset, sizeof(ProcessProtectionPolicy));
	}

	VOID ModifyProcessProtectionPolicy(_In_ PEPROCESS Process, _In_ ProcessProtectionPolicy& NewProtectionPolicy) {
		auto offset = GetProtectionPolicyOffset();

		auto* protectionPolicy = reinterpret_cast<ProcessProtectionPolicy*>(reinterpret_cast<UCHAR*>(Process) + offset);
		::RtlCopyMemory(protectionPolicy, &NewProtectionPolicy, sizeof(ProcessProtectionPolicy));
	}
}
