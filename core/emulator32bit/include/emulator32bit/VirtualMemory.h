#pragma once
#ifndef VIRTUALMEMORY_H
#define VIRTUALMEMORY_H

#include "emulator32bit/Emulator32bitUtil.h"
#include "emulator32bit/Disk.h"

#define VM_PAGE_PSIZE 12
#define VM_PAGE_SIZE (1 << VM_PAGE_SIZE)
#define VM_MAX_PAGES 1024
class VirtualMemory {
	public:
		VirtualMemory(Disk& disk);
		~VirtualMemory();

		struct Exception {
			enum class Type {
				AOK, INVALID_ADDRESS,
			} type = Type::AOK;
			word address = 0;
		};

		virtual word map_address(word address, Exception& exception);

	private:
		struct PageTableEntry {
			word vpage = 0;
			word ppage = 0;
			bool dirty = false;
			bool disk = true;
		};

		struct PageTable {
			long long pid = 0;
			bool valid = false;
			PageTableEntry *entries;
		};

		Disk& m_disk;
		PageTable m_ptable;
};

class MockVirtualMemory : public VirtualMemory {
	public:
		MockVirtualMemory();

		word map_address(word address, Exception& exception) override;
	private:
		MockDisk mockdisk;
};

#endif /* VIRTUALMEMORY_H */