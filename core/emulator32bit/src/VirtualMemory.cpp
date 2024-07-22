#include "emulator32bit/VirtualMemory.h"

VirtualMemory::VirtualMemory(Disk& disk) : m_disk(disk) {

}

VirtualMemory::~VirtualMemory() {
	if (m_ptable.valid) {
		delete[] m_ptable.entries;
	}
}

word VirtualMemory::map_address(word address, Exception& exception) {
	return address;
}

MockVirtualMemory::MockVirtualMemory() : VirtualMemory(mockdisk), mockdisk() {

}

word MockVirtualMemory::map_address(word address, Exception& exception) {
	return address;
}