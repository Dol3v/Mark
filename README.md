# Mark
> :warning: This project is _incredibly_ buggy, unstable, and will probably BSoD for you.

## About
An ~~amazingly bad~~ Windows kernel rootkit, built as a final project for the Cyber track in my highschool.

## Flow

1. The rootkit loads itself via `DriverLoader.exe`, which utilizes [CVE-2020-12446](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2020-12446): an arbitrary physical r/w vulnerability in `ene.sys`
    1. We first convert the physical r/w to virtual r/w: we spray a bunch of `PALETTE`s and look for them in physical memory. We can then modify pointers within them to acheieve virtual r/w[^kernel-rw].
    2. Using the virtual r/w, we can modify an `NtUser*` syscall with matching signature to allocate our kernel shellcode and run it, [Morten-Schenk-style](https://media.defcon.org/DEF%20CON%2025/DEF%20CON%2025%20presentations/DEF%20CON%2025%20-%20Morten-Schenk-Taking-Windows-10-Kernel-Exploitation-to-the-next-level-Leveraging-vulns-in-Creators-Update.pdf). I used `NtUserSetFeatureReportResponse`, which was rarely called on my test machine and had a similar signature to `ExAllocatePoolWithTag`.
    3. We can then employ a technique similar to reflective dll loading, and load our via kernel shellcode. This method leaves no traces in the registry and is 10 times cooler than doing the alternatuive "virtual r/w to `NT AUTHORITY\SYSTEM` to loading driver via `sc.exe`" method.
2. We then inject our `C2` dll to defender's process. Traffic isn't suspicious if it's coming out of the AV, right?
3. The `C2` dll connects to the ~~incomplete~~ python `HttpServer`, and receives commands from it, such as
    * `StartKeylogging` - uses IRP hooking on `kbdclass.sys` to record keystrokes.
    * `EndKeylogging` - self explantory
    * `InjectLibrary` - injects a dll into a (potentially protected) process
    * `RunKmShellcode` - runs km shellcode and returns the result

## Features
The rootkit features several cool features, such as
 * Kernel-mode keylogging
 * C2 and networking
 * Object callbacks removal
 * Reflective driver loading
 * Really ugly uses of `reinterpret_cast`

```cpp
/*
	Checks if a pointer is pointing at a palette, return its index if it is, otherwise return -1
*/
UINT32 GetPaletteIndexFromPointer(const PBYTE Pointer, DWORD Tid) {
	if (*reinterpret_cast<const PALETTEENTRY*>(Pointer) != PALETTE_IDENTIFIER) return false;
	// assume it's pointing at a palette, let's make some extra checks, to be certain
	const auto* palette = reinterpret_cast<const PALETTE64*>(Pointer - FIELD_OFFSET(PALETTE64, apalColors));
	if (palette->cEntries == 2 && palette->BaseObject.Tid == Tid) {
		return *reinterpret_cast<const UINT32*>(&palette->apalColors[1]);
	} 
	return -1;
}
```
_Case in point_

---
### TODOs
 * Convert C2 to HTTPS
 * Convert UM dll loading to reflective loading to avoid appearing on `PEB`
 * Convert KM shellcode running to reflective driver loading
 * Add hooks on AV's `FilterCommunicationPort`s for evasion
 * Add heartbeat functionality
 * Finally get to work on weaponizing an SMI handler exploit for an HVCI bypass
 * idk, persistence? :joy:

## Why so bad?
Here are some points for improvement:
 * The load-library from remote feature is cool, but it literally drops the dll to `%TEMP%`
 * The kernel-keylogging method employed is easily detected[^kernel-keylogging-dis].

I'll leave the rest for you to find:smile:
>  :poop: Also a non-trivial portion of it was written hastily 2 days before the deadline, so it isn't exactly production-ready.

## Why it's called Mark
Ask [mark](https://github.com/KO3JlUHA).

## Disclaimer
The project was built and meant to be used for educational purposes only.
Also if you're using someone's extremely buggy and feature-less highschool project in malware something's definitely wrong with you.

[^kernel-rw]: See the end of https://www.zerodayinitiative.com/blog/2019/12/16/local-privilege-escalation-in-win32ksys-through-indexed-color-palettes

[^kernel-keylogging-dis]: And already known by a bunch of AV vendors; see (the awesome post) https://securelist.com/keyloggers-implementing-keyloggers-in-windows-part-two/36358/