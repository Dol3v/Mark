#pragma once

#include <ntifs.h>

/*
	The function that runs the shellcode.
*/
extern "C" void RunShellcode();

/*
	Returns the shellcode's size.
*/
extern "C" ULONG GetShellcodeSize();
