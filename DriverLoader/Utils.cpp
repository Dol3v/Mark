
#include "Utils.h"

#include <Psapi.h>
#include <DbgHelp.h>
#include <array>


std::tuple<ULONG64, ULONG64> PeUtils::GetKernelModuleAddresses() {
	std::array<PVOID, 512> drivers;
	DWORD needed = 0;
	if (!EnumDeviceDrivers(drivers.data(), drivers.size(), &needed) || needed > drivers.size())
		return std::make_tuple(0, 0);

	CHAR driverName[1024] = { 0 };
	PVOID win32kbaseAddress = nullptr, ntoskrnlBase = nullptr;
	for (auto baseAddress : drivers) {
		auto length = GetDeviceDriverBaseNameA(baseAddress, driverName, sizeof(driverName));
		if (length == 0 || length == sizeof(driverName)) continue;
		std::string driverNameWrapper(driverName);
		if (driverNameWrapper.find("win32kbase.sys") != std::string::npos)
			win32kbaseAddress = baseAddress;
		if (driverNameWrapper.find("ntoskrnl.exe") != std::string::npos)
			ntoskrnlBase = baseAddress;

		if (win32kbaseAddress != nullptr && ntoskrnlBase != nullptr)
			break;
	}
	return std::make_tuple(reinterpret_cast<ULONG64>(win32kbaseAddress), reinterpret_cast<ULONG64>(ntoskrnlBase));
}

PVOID PeUtils::GetExportAddress(PBYTE ImageBase, const char* ExportName) {
	auto* ntHeader = ImageNtHeader(ImageBase);
	auto* exportDirectory = reinterpret_cast<PIMAGE_EXPORT_DIRECTORY>(ImageBase + ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
	
	auto* addresses = reinterpret_cast<PDWORD>(ImageBase+exportDirectory->AddressOfFunctions);
	auto* names = reinterpret_cast<PDWORD>(ImageBase + exportDirectory->AddressOfNames);
	auto* ordinals = reinterpret_cast<PWORD>(ImageBase + exportDirectory->AddressOfNameOrdinals);

	for (auto i = 0; i < exportDirectory->NumberOfFunctions; ++i) {
		if (!strcmp(ExportName, reinterpret_cast<char*>(ImageBase + names[i]))) {
			return ImageBase + addresses[ordinals[i]];
		}
	}
	return nullptr;
}

DWORD PeUtils::GetImageSize(PVOID ImageBase) {
	auto* ntHeader = ImageNtHeader(ImageBase);
	return ntHeader->OptionalHeader.SizeOfImage;
}