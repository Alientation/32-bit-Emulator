#pragma once
#ifndef VIRTUAL_MEMORY_H
#define VIRTUAL_MEMORY_H

#include "emulator32bit/emulator32bit_util.h"
#include "emulator32bit/disk.h"
#include "emulator32bit/fbl.h"

#include <unordered_map>

#define VM_MAX_PAGES 1024
class VirtualMemory
{
	public:
		VirtualMemory(word ram_start_page, word ram_end_page, Disk& disk);
		virtual ~VirtualMemory();

		Disk& m_disk;

		struct Exception {
			enum class Type {							// todo  VVVV VERY UGLY AND BAD, FIX
				AOK, INVALID_ADDRESS, DISK_FETCH_SUCCESS, DISK_RETURN_AND_FETCH_SUCCESS, DISK_FETCH_FAILED,
			} type = Type::AOK;
			word address = 0;
			std::vector<byte> disk_fetch;
			word ppage_fetch;
			word ppage_return;
			word disk_page_return;
		}; // todo, maybe exceptions should be unions instead?? since different exceptions have different values to store

		virtual void set_process(long long pid);
		virtual void begin_process(long long pid, word alloc_mem_begin = 0, word alloc_mem_end = PAGE_SIZE-1);
		virtual void end_process(long long pid);
		virtual word map_address(word address, Exception& exception);

	private:
		struct PageTableEntry {
			word vpage = 0;
			word ppage = 0;
			bool dirty = false;		// todo, unused, not sure if it is necessary (maybe instead don't free up disk space unless we need to)
			bool disk = true;
			word diskpage = 0;
		};

		struct PageTable {
			long long pid = 0;
			std::unordered_map<word, PageTableEntry*> entries = std::unordered_map<word,PageTableEntry*>();
		};

		word m_ram_start_page;
		word m_ram_end_page;
		std::unordered_map<long long, PageTable*> m_process_ptable_map;
		std::unordered_map<word, PageTableEntry*> m_physical_memory_map;

		FreeBlockList m_freelist;

		PageTable *m_cur_ptable = nullptr;

		/* LRU */
		struct LRU_Node {
			word ppage;
			LRU_Node* next;
			LRU_Node* prev;
		};
		LRU_Node* m_lru_head = nullptr;
		LRU_Node* m_lru_tail = nullptr;
		std::unordered_map<word,LRU_Node*> m_lru_map;

		void check_vm();

		void add_lru(word ppage);
		word remove_lru();
		void check_lru();

		void add_page(word vpage); // todo add pid arg
		void remove_page(long long pid, word vpage);
		word access_page(word vpage, Exception& exception); // todo add pid arg
};

class MockVirtualMemory : public VirtualMemory
{
	public:
		MockVirtualMemory(word ram_start_page, word ram_end_page);
		~MockVirtualMemory() override;

		void set_process(long long pid) override;
		void begin_process(long long pid, word alloc_mem_begin = 0, word alloc_mem_end = PAGE_SIZE-1) override;
		void end_process(long long pid) override;
		word map_address(word address, Exception& exception) override;
	private:
		MockDisk mockdisk;
};

#endif /* VIRTUAL_MEMORY_H */