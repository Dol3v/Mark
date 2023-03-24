#pragma once

#include <ntifs.h>

// Undocumented APC structs
typedef enum _KAPC_ENVIRONMENT {
	OriginalApcEnvironment,
	AttachedApcEnvironment,
	CurrentApcEnvironment,
	InsertApcEnvironment
} KAPC_ENVIRONMENT;

typedef VOID(*PKNORMAL_ROUTINE) (
	IN PVOID NormalContext,
	IN PVOID SystemArgument1,
	IN PVOID SystemArgument2
	);

typedef VOID(*PKKERNEL_ROUTINE) (
	IN PKAPC Apc,
	IN OUT PKNORMAL_ROUTINE* NormalRoutine,
	IN OUT PVOID* NormalContext,
	IN OUT PVOID* SystemArgument1,
	IN OUT PVOID* SystemArgument2
	);

typedef VOID(*PKRUNDOWN_ROUTINE) (
	IN  PKAPC Apc
	);

extern "C"
VOID KeInitializeApc(
	IN  PKAPC Apc,
	IN  PKTHREAD Thread,
	IN  KAPC_ENVIRONMENT Environment,
	IN  PKKERNEL_ROUTINE KernelRoutine,			   // executed at kernel apc level(IRQL APC_LEVEL)
	IN  PKRUNDOWN_ROUTINE RundownRoutine OPTIONAL, // executed when thread is terminating
	IN  PKNORMAL_ROUTINE NormalRoutine OPTIONAL,   // executed at PASSIVE_LEVEL(IRQL 0) in ProcessorMode 
	IN  KPROCESSOR_MODE ApcMode OPTIONAL,		   // Supplies the ProcessorMode in which the function specified by the NormalRoutine parameter is to be executed.
	IN  PVOID NormalContext OPTIONAL
);

extern "C"
BOOLEAN KeInsertQueueApc(
	IN  PKAPC Apc,
	IN  PVOID SystemArgument1,
	IN  PVOID SystemArgument2,
	IN  KPRIORITY Increment
);

/*
	Queues an APC in a target thread.
*/
NTSTATUS QueueApc(PKNORMAL_ROUTINE ApcRoutine, KPROCESSOR_MODE ApcProcessorMode, PETHREAD TargetThread, PVOID SystemArgument1 = nullptr, PVOID SystemArgument2 = nullptr);
