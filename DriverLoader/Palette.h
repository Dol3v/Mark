#pragma once

#include <Windows.h>
#include <wingdi.h>
#include <WinUser.h>

/*
	Functions/definitions relating to palettes
*/

typedef struct {
    ULONG64 hHmgr;
    ULONG32 ulShareCount;
    WORD cExclusiveLock;
    WORD BaseFlags;
    ULONG64 Tid;
} BASEOBJECT64;

typedef struct _PALETTE64
{
    BASEOBJECT64      BaseObject;    // 0x00
    FLONG           flPal;         // 0x18
    ULONG32           cEntries;      // 0x1C
    ULONG32           ulTime;        // 0x20 
    ULONG32             hdcHead;       // 0x24
    ULONG64        hSelected;     // 0x28, 
    ULONG64           cRefhpal;      // 0x30
    ULONG64          cRefRegular;   // 0x34
    ULONG64      ptransFore;    // 0x3c
    ULONG64      ptransCurrent; // 0x44
    ULONG64      ptransOld;     // 0x4C
    ULONG32           unk_038;       // 0x38
    ULONG64         pfnGetNearest; // 0x3c
    ULONG64   pfnGetMatch;   // 0x40
    ULONG64           ulRGBTime;     // 0x44
    ULONG64       pRGBXlate;     // 0x48
    PALETTEENTRY* pFirstColor;  // 0x80
    struct _PALETTE* ppalThis;     // 0x88
    PALETTEENTRY    apalColors[3]; // 0x90
} PALETTE64;
