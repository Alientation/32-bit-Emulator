#pragma once
#ifndef VIRTUALMEMORY_H
#define VIRTUALMEMORY_H

#include "emulator32bit/Emulator32bitUtil.h"
#include "emulator32bit/Disk.h"

#include <unordered_map>

#define VM_PAGE_PSIZE 12
#define VM_PAGE_SIZE (1 << VM_PAGE_PSIZE)
#define VM_MAX_PAGES 1024
class VirtualMemory {
	public:
		VirtualMemory(Disk& disk);

		struct Exception {
			enum class Type {
				AOK, INVALID_ADDRESS,
			} type = Type::AOK;
			word address = 0;
		};

		virtual void begin_process(long long pid, word alloc_mem_begin = 0, word alloc_mem_end = VM_PAGE_SIZE-1);
		virtual void end_process(long long pid);
		virtual word map_address(word address, Exception& exception);

	private:
		struct PageTableEntry {
			word vpage = 0;
			word ppage = 0;
			bool dirty = false;
			bool disk = true;
			word diskpage = 0;
		};

		struct PageTable {
			long long pid = 0;
			std::unordered_map<word, PageTableEntry> entries;
		};

		Disk& m_disk;
		std::unordered_map<long long, PageTable*> m_ptable_map;
		PageTable *m_ptable = nullptr;
};

class MockVirtualMemory : public VirtualMemory {
	public:
		MockVirtualMemory();

		void begin_process(long long pid, word alloc_mem_begin = 0, word alloc_mem_end = VM_PAGE_SIZE-1) override;
		void end_process(long long pid) override;
		word map_address(word address, Exception& exception) override;
	private:
		MockDisk mockdisk;
};

#endif /* VIRTUALMEMORY_H */