#include <Windows.h>

#include <iostream>
#include <WinUser.h>
#include <wtypes.h>
#include <tuple>
//typedef struct _POINT {
//	int x;
//	int y;
//} POINT, * PPOINT;


std::ostream& operator<< (std::ostream& stream, const POINT& point) {
	stream << "x: " << point.x << " y: " << point.y;
	return stream;
}

bool GetScreenResolution(ULONG& x, ULONG& y) {
	/*RECT desktop;
	const HWND desktopHandle = GetDesktopWindow();
	if (!GetWindowRect(desktopHandle, &desktop)) {
		std::cout << "Failed to get desktop dimensions, error=" << std::hex << GetLastError() << std::endl;
		return false;
	}
	std::cout << desktop.left << " " << desktop.right << " " << desktop.bottom << " " << desktop.top << std::endl;*/

	/*x = desktop.right;
	y = desktop.bottom;*/

	x = 
	y = GetSystemMetrics(SM_CYSCREEN);
	return true;
}

int main() {
	auto* cursorPos = new POINT{ 0 };
	if (!GetCursorPos(cursorPos)) {
		std::cout << "Failed to query mouse position, error code=" << std::hex << GetLastError() << std::endl;
		return -1;
	}
	std::cout << *cursorPos << std::endl;
	ULONG x, y;
	if (!GetScreenResolution(x, y)) {
		return -1;
	}
	std::cout << "Screen width: " << x << " Screen Height: " << y << std::endl;
}	
