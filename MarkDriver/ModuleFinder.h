#pragma once

#include <ntifs.h>
#include <aux_klib.h>

#include "Macros.h"

/*
	Finds the module a kernelmode address is found in.
*/
class ModuleFinder {

private:
	AUX_MODULE_EXTENDED_INFO* modules = nullptr;
	ULONG numberOfModules = 0;

	void Initialize() {
		auto status = STATUS_SUCCESS;

		if (!NT_SUCCESS(status = ::AuxKlibInitialize())) {
			LOG_FAIL("Failed to initialize auxklib");
			::ExRaiseStatus(status);
		}

		ULONG moduleSize = 0;
		if (!NT_SUCCESS(status = ::AuxKlibQueryModuleInformation(&moduleSize, sizeof(AUX_MODULE_EXTENDED_INFO), nullptr))) {
			LOG_FAIL("Failed to get module info size");
			::ExRaiseStatus(status);
		}
		auto* moduleInfoBuf = ::ExAllocatePoolWithTag(PagedPool, moduleSize, DRIVER_TAG);
		if (moduleInfoBuf == nullptr) {
			LOG_FAIL("Failed to allocate buffer");
			status = STATUS_INSUFFICIENT_RESOURCES;
			::ExRaiseStatus(status);
		}
		if (!NT_SUCCESS(status = ::AuxKlibQueryModuleInformation(&moduleSize, sizeof(AUX_MODULE_EXTENDED_INFO), moduleInfoBuf))) {
			LOG_FAIL("Failed to get module info");
			::ExFreePoolWithTag(moduleInfoBuf, DRIVER_TAG);
			::ExRaiseStatus(status);
		}
		this->modules = static_cast<AUX_MODULE_EXTENDED_INFO*>(moduleInfoBuf);
		this->numberOfModules = moduleSize / sizeof(AUX_MODULE_EXTENDED_INFO);
		LOG_SUCCESS_VA("Found %ul modules", this->numberOfModules);
	}

	void Destruct() {
		if (this->modules != nullptr) {
			::ExFreePoolWithTag(this->modules, DRIVER_TAG);
		}
	}

public:
	/*
		Initializes the module finder: stores all currently loaded modules.

		Raises an exception if initialization failed.
	*/
	ModuleFinder::ModuleFinder() {
		this->Initialize();
	}

	/*
		Refreshes currently loaded modules.
	*/
	void Refresh() {
		this->Destruct();
		this->Initialize();
	}

	/*
		Locates the module an address resides in.
	*/
	AUX_MODULE_EXTENDED_INFO* FindModuleByAddress(PVOID Address) const {
		for (ULONG i = 0; i < this->numberOfModules; ++i) {
			auto& currentModule = this->modules[i];
			if (currentModule.BasicInfo.ImageBase < Address && reinterpret_cast<UCHAR*>(currentModule.BasicInfo.ImageBase) + currentModule.ImageSize > Address) {
				return &currentModule;
			}
		}
		return nullptr;
	}

	/*
		Returns the base address of a module by its name.
	*/
	PVOID GetModuleBaseByName(const char* Name) {
		for (ULONG i = 0; i < this->numberOfModules; ++i) {
			auto& currentModule = this->modules[i];
			if (!::strcmp(reinterpret_cast<const char*>(currentModule.FullPathName) + currentModule.FileNameOffset, Name)) {
				return currentModule.BasicInfo.ImageBase;
			}
		}
		return nullptr;
	}

	const AUX_MODULE_EXTENDED_INFO* get() {
		return this->modules;
	}

	ModuleFinder::~ModuleFinder() {
		this->Destruct();
	}
};