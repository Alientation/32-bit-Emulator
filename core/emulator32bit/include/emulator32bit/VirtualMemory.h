#pragma once
#ifndef VIRTUALMEMORY_H
#define VIRTUALMEMORY_H

#include "emulator32bit/Emulator32bitUtil.h"
#include "emulator32bit/Disk.h"

#include <unordered_map>

#define VM_MAX_PAGES 1024
class VirtualMemory {
	public:
		VirtualMemory(word ram_start_page, word ram_end_page, Disk& disk);

		struct Exception {
			enum class Type {
				AOK, INVALID_ADDRESS,
			} type = Type::AOK;
			word address = 0;
		};

		virtual void set_process(long long pid);
		virtual void begin_process(long long pid, word alloc_mem_begin = 0, word alloc_mem_end = PAGE_SIZE-1);
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

		word m_ram_start_page;
		word m_ram_end_page;
		Disk& m_disk;
		std::unordered_map<long long, PageTable*> m_process_ptable_map;
		std::unordered_map<word, PageTableEntry*> m_physical_memory_map;

		struct FreePhysicalPage {
			word paddr;
			word len;
			FreePhysicalPage *next;
		};
		FreePhysicalPage *head;

		PageTable *m_cur_ptable = nullptr;

		void add_page(word vpage);
		void remove_page(word vpage);
		word access_page(word vpage);
};

class MockVirtualMemory : public VirtualMemory {
	public:
		MockVirtualMemory(word ram_start_page, word ram_end_page);

		void set_process(long long pid) override;
		void begin_process(long long pid, word alloc_mem_begin = 0, word alloc_mem_end = PAGE_SIZE-1) override;
		void end_process(long long pid) override;
		word map_address(word address, Exception& exception) override;
	private:
		MockDisk mockdisk;
};

#endif /* VIRTUALMEMORY_H */