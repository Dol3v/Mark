#pragma once

// protection related structs

// nonstandard extension used: nameless struct/union
#pragma warning(disable: 4201)


namespace Ppl {
    typedef struct _PS_PROTECTION {
        union {
            UCHAR Level;
            struct {
                UCHAR Type : 3;
                UCHAR Audit : 1;                  // Reserved
                UCHAR Signer : 4;
            };
        };
    } PS_PROTECTION, * PPS_PROTECTION;

    /*
        Process protection-related settings.
    */
    #pragma pack(push, 1)
    typedef struct {
        // Exe signature level
        UCHAR SignatureLevel;
        // Minimum signature level of loaded images
        UCHAR SectionSignatureLevel;
        // Protection level of process (e.g. Tcb, Windows, ProtectedProcessLight)
        PS_PROTECTION Protection;
    } ProcessProtectionPolicy;
    #pragma pack(pop)
    /*
        Modify the protection policy of the current process.

        Can raise exceptions, see GetPsProtectionOffset.
    */
    VOID ModifyProcessProtectionPolicy(_In_ PEPROCESS Process, _In_ ProcessProtectionPolicy& ProtectionPolicy);

    /*
        Returns the current protection level of the current process.

        Can raise exceptions, see GetPsProtectionOffset.
    */
    VOID GetProcessProtection(_In_ const PEPROCESS Process, _Inout_ ProcessProtectionPolicy* ProtectionPolicy);
}