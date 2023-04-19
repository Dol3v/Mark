
#include "ReflectiveLoader.h"
#include "DriverConfiguration.h"

#include <intrin.h>
#include <ntimage.h>

namespace Loader {

	VOID RelectiveLoader(LoaderContext* Context) {
		Functions funcs = { 0 };

		// bunch of stack strings
		DECLARE_UNICODE_STRING(uExAllocatePoolWithTag, L"ExAllocatePoolWithTag");
		DECLARE_UNICODE_STRING(uExFreePoolWithTag, L"ExFreePoolWithTag");
		DECLARE_UNICODE_STRING(uIoCreateDriver, L"IoCreateDriver");
		DECLARE_UNICODE_STRING(uRtlImageDirectoryEntryToData, L"RtlImageDirectoryEntryToData");
		DECLARE_UNICODE_STRING(uRtlImageNtHeader, L"RtlImageNtHeader");
		DECLARE_UNICODE_STRING(uRtlQueryModuleInformation, L"RtlQueryModuleInformation");
		DECLARE_UNICODE_STRING(uDriverName, L"\\Drivers\\Mark");
		
		// initialize functions
		funcs.MmGetSystemRoutineAddress = Context->MmGetSystemRoutineAddress;
		funcs.ExAllocatePoolWithTag = (ExAllocatePoolWithTag_t)Context->MmGetSystemRoutineAddress(&uExAllocatePoolWithTag);
		funcs.ExFreePoolWithTag = (ExFreePoolWithTag_t)Context->MmGetSystemRoutineAddress(&uExAllocatePoolWithTag);
		funcs.IoCreateDriver = (IoCreateDriver_t)Context->MmGetSystemRoutineAddress(&uIoCreateDriver);
		funcs.RtlImageDirectoryEntryToData = (RtlImageDirectoryEntryToData_t)Context->MmGetSystemRoutineAddress(&uRtlImageDirectoryEntryToData);
		funcs.RtlImageNtHeader = (RtlImageNtHeader_t)Context->MmGetSystemRoutineAddress(&uRtlImageNtHeader);
		funcs.RtlQueryModuleInformation = (RtlQueryModuleInformation_t)Context->MmGetSystemRoutineAddress(&uRtlQueryModuleInformation);
	
		for (int i = 0; i < sizeof(funcs); i += sizeof(PVOID)) {
			if (*(((PVOID*)&funcs) + i) == nullptr) {
				return;
			}
		}

		// copy headers & sections, apply relocations, resolve imports and load entry
		
		auto* ntHeader = funcs.RtlImageNtHeader(Context->ImageBase);
		auto* loadedImage = (PUCHAR)funcs.ExAllocatePoolWithTag(PagedPool, ntHeader->OptionalHeader.SizeOfImage, DRIVER_TAG);
		if (loadedImage == nullptr) return;

		// copy headers
		__movsq((PULONG64)loadedImage, (PULONG64)Context->ImageBase, ntHeader->OptionalHeader.SizeOfHeaders / sizeof(__int64));
		
		// copy sections
		auto* sections = IMAGE_FIRST_SECTION(ntHeader);
		for (auto i = 0; i < ntHeader->FileHeader.NumberOfSections; ++i) {
			auto rva = sections[i].VirtualAddress;
			__movsq((PULONG64)(loadedImage + rva), (PULONG64)((PUCHAR)Context->ImageBase + rva), sections[i].SizeOfRawData);
		}

		if (!NT_SUCCESS(ApplyRelocations(&funcs, loadedImage))) {
			funcs.ExFreePoolWithTag(loadedImage, DRIVER_TAG);
			return;
		}
		if (!NT_SUCCESS(LoadImports(&funcs, loadedImage))) {
			funcs.ExFreePoolWithTag(loadedImage, DRIVER_TAG);
			return;
		}

		auto* entry = (PDRIVER_INITIALIZE)(loadedImage + ntHeader->OptionalHeader.AddressOfEntryPoint);
		funcs.IoCreateDriver(&uDriverName, entry);
	}

	NTSTATUS ApplyRelocations(Functions* Funcs, PUCHAR ImageBase) {
		NTSTATUS status = STATUS_UNSUCCESSFUL;
		ULONG size;
		PIMAGE_NT_HEADERS pNTHdr = Funcs->RtlImageNtHeader(ImageBase);
		ULONGLONG delta = (ULONGLONG)ImageBase - pNTHdr->OptionalHeader.ImageBase;
		PIMAGE_BASE_RELOCATION pRel = (PIMAGE_BASE_RELOCATION)Funcs->RtlImageDirectoryEntryToData(ImageBase, TRUE, IMAGE_DIRECTORY_ENTRY_BASERELOC, &size);
		if (pRel) {
			size = pNTHdr->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size;
			for (ULONG i = 0; i < size; i += pRel->SizeOfBlock, pRel = (PIMAGE_BASE_RELOCATION)((PUCHAR)pRel + i))
			{
				for (PUSHORT chains = (PUSHORT)((ULONGLONG)pRel + sizeof(IMAGE_BASE_RELOCATION)); chains < (PUSHORT)((ULONGLONG)pRel + pRel->SizeOfBlock); ++chains)
				{
					switch (*chains >> 12)
					{
					case IMAGE_REL_BASED_ABSOLUTE:
						break;
					case IMAGE_REL_BASED_HIGHLOW:
						*(PULONG)(ImageBase + pRel->VirtualAddress + (*chains & 0x0fff)) += (ULONG)delta;
						break;
					case IMAGE_REL_BASED_DIR64:
						*(PULONGLONG)(ImageBase + pRel->VirtualAddress + (*chains & 0x0fff)) += delta;
						break;
					default:
						return STATUS_NOT_IMPLEMENTED;
					}
				}
			}
			status = STATUS_SUCCESS;
		}
		return status;
	}

	NTSTATUS LoadImports(Functions* Funcs, PUCHAR ImageBase) {
		NTSTATUS status = STATUS_UNSUCCESSFUL;
		ULONG size = 0;
		auto* importDescriptor = reinterpret_cast<PIMAGE_IMPORT_DESCRIPTOR>(Funcs->RtlImageDirectoryEntryToData(ImageBase, TRUE, IMAGE_DIRECTORY_ENTRY_IMPORT, &size));
		if (importDescriptor)
		{
			for (; importDescriptor->Name; importDescriptor++)
			{
				LPSTR libName = (LPSTR)(ImageBase + importDescriptor->Name);
				auto* moduleBase = (PUCHAR) GetModuleByName(Funcs, libName);
				if (moduleBase) {
					auto* functionNames = (PIMAGE_THUNK_DATA)(ImageBase + importDescriptor->OriginalFirstThunk); // ILT
					auto* functionAddresses = (PIMAGE_THUNK_DATA)(ImageBase + importDescriptor->FirstThunk); // IAT

					for (; functionNames->u1.ForwarderString; ++functionNames, ++functionAddresses)
					{
						auto* pIName = (PIMAGE_IMPORT_BY_NAME)(ImageBase + functionNames->u1.AddressOfData);
						auto* importedFunc = GetFunctionByName(Funcs, moduleBase, pIName->Name);
						if (importedFunc)
							functionAddresses->u1.Function = (ULONGLONG)importedFunc;
						else return STATUS_PROCEDURE_NOT_FOUND;
					}
				}
				else return STATUS_DRIVER_UNABLE_TO_LOAD;
			}
			status = STATUS_SUCCESS;
		}
		return status;
	}

	/*
		Can't do external calls :(
	*/

	__forceinline INT CompareStrings(const char* First, const char* Second) {
		char a = '\0', b = '\0';
		while ((a = *(First++)) != 0 && (b = *(Second++)) != 0) {
			if (a != b) {
				return a - b;
			}
		}
		if (a != 0) return a;
		return b;
	}

	__forceinline CHAR ToLower(CHAR a) {
		if (a >= 'A' && a <= 'Z') return a + ('a' - 'A');
		return a;
	}

	__forceinline INT CompareStringsCaseInsensitive(const char* First, const char* Second) {
		char a = '\0', b = '\0';
		while ((a = ToLower(*(First++))) != 0 && (b = ToLower(*(Second++))) != 0) {
			if (a != b) {
				return a - b;
			}
		}
		return a - b;
	}

	PVOID GetFunctionByName(Functions* Funcs, PUCHAR ImageBase, const CHAR* FunctionName) {
		ULONG exportDirectorySize = 0;
		auto* exportDirectory = (PIMAGE_EXPORT_DIRECTORY)Funcs->RtlImageDirectoryEntryToData(ImageBase, true, IMAGE_DIRECTORY_ENTRY_EXPORT, &exportDirectorySize);
		auto* names = (PULONG)(ImageBase + exportDirectory->AddressOfNames);
		auto* ordinals = (PUSHORT)(ImageBase + exportDirectory->AddressOfNameOrdinals);
		auto* functions = (PULONG)(ImageBase + exportDirectory->AddressOfFunctions);
		for (ULONG i = 0; i < exportDirectory->NumberOfNames; ++i)
		{
			LPCSTR name = (LPCSTR)(ImageBase + names[i]);
			if (!CompareStrings(FunctionName, name))
			{
				return ImageBase + functions[ordinals[i]];
			}
		}
		return nullptr;
	}

	PVOID GetModuleByName(Functions* Funcs, const CHAR* ModuleName) {
		ULONG size = 0;
		PVOID ImageBase = NULL;
		NTSTATUS status = Funcs->RtlQueryModuleInformation(&size, sizeof(RTL_MODULE_EXTENDED_INFO), NULL);
		if NT_SUCCESS(status) {
			PRTL_MODULE_EXTENDED_INFO modules = (PRTL_MODULE_EXTENDED_INFO)Funcs->ExAllocatePoolWithTag(PagedPool, size, DRIVER_TAG);
			if (modules) {
				status = Funcs->RtlQueryModuleInformation(&size, sizeof(RTL_MODULE_EXTENDED_INFO), modules);
				if NT_SUCCESS(status) {
					for (ULONG i = 0; i < size / sizeof(RTL_MODULE_EXTENDED_INFO); ++i) {
						if (CompareStringsCaseInsensitive(ModuleName, &modules[i].FullPathName[modules[i].FileNameOffset])) {
							ImageBase = modules[i].ImageBase;
							break;
						}
					}
				}
				Funcs->ExFreePoolWithTag(modules, DRIVER_TAG);
			}
		}
		return ImageBase;
	}
}