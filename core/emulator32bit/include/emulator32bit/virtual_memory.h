#pragma once
#ifndef VIRTUAL_MEMORY_H
#define VIRTUAL_MEMORY_H

#define AEMU_ONLY_CRITICAL_LOG

#include "emulator32bit/emulator32bit_util.h"
#include "emulator32bit/disk.h"
#include "emulator32bit/fbl.h"
#include "util/loggerv2.h"

#include <unordered_map>

/*
IDEA
instead of using unordered maps, use just a massive array instead.. might take more memory,
but who cares for now, lets have a TLB instead, to have fast access on average to pages
instead of having to go to the unordered maps

*/

#define VM_MAX_PAGES 1024
#define TLB_PSIZE 12
#define TLB_SIZE (1 << TLB_PSIZE)
class VirtualMemory
{
	public:
		VirtualMemory(word ram_start_page, word ram_end_page, Disk& disk);
		virtual ~VirtualMemory();

		Disk& m_disk;
		bool enabled = true;				/* Whether addresses should be mapped. */

		class VirtualMemoryException : public std::exception
		{
			private:
				std::string message;

			public:
				VirtualMemoryException(const std::string& msg);

				const char* what() const noexcept override;
		};

		/**
		 * @brief 			Represents recoverable exception states that should be handled by the caller.
		 *
		 * @note 			INVALID_ADDRESS and DISK_FETCH_FAILED should be instead through as a c++ exception.
		 * 					These should not be recovered from and the running user program should exit
		 * 					(or the kernel should just crash???).
		 */
		struct Exception {
			enum class Type {
				AOK, INVALID_ADDRESS, DISK_FETCH_SUCCESS, DISK_RETURN_AND_FETCH_SUCCESS, DISK_FETCH_FAILED,
			};

			Type type = Type::AOK;
			word address = 0;						/* Address accessed */

			/*
			 * Result of the fetched disk if DISK_FETCH_SUCCESS or DISK_RETURN_AND_FETCH_SUCCESS
			 * to be written to memory at the physical ppage_fetch.
			 *
			 * Note: perhaps instead of supplying the disk data, just give the disk page to read
			 * and write to the specified physical memory page.
			*/
			std::vector<byte> disk_fetch;
			word ppage_fetch;						/* physical page to write the disk fetch results to. */
			word ppage_return;						/* physical page to read from and write to disk at disk_page_return. */
			word disk_page_return;					/* disk page to write the read physical page to. */
		};

		/**
		 * @brief 			Sets the current proccess to change the virtual space mappings.
		 *
		 * @param pid 		Process id.
		 */
		void set_process(long long pid);

		/**
		 * @brief 			Starts a new processes with the specified virtual memory allocated
		 * @note			Change the specified virtual memory space range to be in terms of pages.
		 * @note			Should not have the kernel supply a process id. Instead find a new pid
		 * 					and return it to the kernel.
		 *
		 * @param pid		Process id.
		 * @param alloc_mem_begin Start virtual address in bytes.
		 * @param alloc_mem_end End virtual address in bytes.
		 */
		void begin_process(long long pid, word alloc_mem_begin = 0, word alloc_mem_end = PAGE_SIZE-1);

		/**
		 * @brief 			Ends a specified process.
		 *
		 * @param pid		Process id.
		 */
		void end_process(long long pid);

		/*
		idea

		allow multiple virtual addresses to map to a specific physical address
		we want to be able to map virtual addresses to ram, rom, disk, i/o, ports, etc (memory mapped)
			-> The physical address space needs to encompass all memory addresses, not just ram..
		Throw a segfault whenever a virtual address page is accessed but not mapped
		Manually map new virtual address pages to a specific range of addresses (possibly supplied with the memory object reference)
			-> this will map a specific virtual address page to an unused physical page. would also mean we need to be able to evict pages
			-> FBL's need to be updated to support this if it doesn't yet
		Manually map a specific virtual address page to a specific physical page
			-> this won't require a unique virtual address page mapping to physical page, multiple other virtual address pages can map to the same phyiscal page
			-> to prevent this from breaking evictions, should we just update the PageTableEntry to support multiple vpages
		*/


		inline word map_address(word address, Exception& exception)
		{
			if (m_cur_ptable == nullptr || !enabled)
			{
				return address;
			}

			DEBUG_SS(std::stringstream() << "Mapping address " << std::to_string(address) << ".");

			word vpage = address >> PAGE_PSIZE;
			word ppage = access_vpage(vpage, exception);
			DEBUG_SS(std::stringstream() << "Accessing virtual memory page " << std::to_string(vpage)
					<< " which is physical page " << std::to_string(ppage) << ".");

			return (ppage << PAGE_PSIZE) + (address & (PAGE_SIZE-1));
		}

		inline void ensure_physical_page_mapping(word vpage, word ppage, Exception& exception)
		{
			if (m_cur_ptable == nullptr || !enabled)
			{
				return;
			}

			if (m_cur_ptable->entries.find(vpage) != m_cur_ptable->entries.end())
			{
				if (m_cur_ptable->entries.at(vpage)->ppage == ppage)
				{
					return;
				}

				ERROR_SS(std::stringstream() << "Virtual page " << std::to_string(vpage)
						<< " is already mapped to a different physical page "
						<< std::to_string(m_cur_ptable->entries.at(vpage)->ppage));
			}

			DEBUG_SS(std::stringstream() << "Mapping physical page " << std::to_string(ppage)
					<< " to virtual page " << std::to_string(vpage) << ".");

			map_ppage(vpage, ppage, exception);
		}


	private:
		struct PageTableEntry {
			PageTableEntry(word vpage, word diskpage);

			std::vector<word> vpages;
			word ppage;
			bool disk;
			word diskpage;
			bool mapped;
			word mapped_ppage;
		};

		struct PageTable {
			long long pid = 0;
			std::unordered_map<word, PageTableEntry*> entries = std::unordered_map<word,PageTableEntry*>();
		};

		struct TLB_Entry {
			bool valid = false;
			long long pid = -1;
			word vpage;
			word ppage;
		};
		TLB_Entry tlb[TLB_SIZE];

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

		bool is_ppage_used(word ppage);

		void evict_ppage(word ppage, Exception& exception);
		void map_vpage_to_ppage(word vpage, word ppage, Exception& exception);

		void add_vpage(word vpage);
		void map_ppage(word vpage, word ppage, Exception& exception);
		void remove_vpage(long long pid, word vpage);

		inline word access_vpage(word vpage, Exception& exception)
		{
			// check_vm();

			word tlb_addr = vpage & (TLB_SIZE-1);
			if (!tlb[tlb_addr].valid || tlb[tlb_addr].pid != m_cur_ptable->pid || tlb[tlb_addr].vpage != vpage)
			{
				if (m_cur_ptable->entries.find(vpage) == m_cur_ptable->entries.end())
				{
					/* Allocate new page to this process */
					add_vpage(vpage);
				}
				else if (!m_cur_ptable->entries.at(vpage)->disk)
				{
					tlb[tlb_addr].valid = true;
					tlb[tlb_addr].pid = m_cur_ptable->pid;
					tlb[tlb_addr].vpage = vpage;
					tlb[tlb_addr].ppage = m_cur_ptable->entries.at(vpage)->ppage;
				}
			}
			else
			{
				return tlb[tlb_addr].ppage;
			}

			PageTableEntry *entry = m_cur_ptable->entries.at(vpage);
			if (!entry->disk)
			{
				DEBUG_SS(std::stringstream() << "accessing virtual page (NOT ON DISK) "
						<< std::to_string(vpage) << " (maps to " << std::to_string(entry->ppage) << ")"
						<< " of process " << std::to_string(m_cur_ptable->pid));
				return entry->ppage;
			}

			DEBUG("BRINGING PAGE ONTO RAM");

			/* Bring page from disk onto RAM */
			FreeBlockList::Exception fbl_exception;
			if (entry->mapped)
			{
				if (is_ppage_used(entry->mapped_ppage))
				{
					evict_ppage(entry->mapped_ppage, exception);
				}

				map_vpage_to_ppage(vpage, entry->mapped_ppage, exception);
			}
			else
			{
				if (!m_freelist.can_fit(1))
				{
					evict_ppage(remove_lru(), exception);
				}

				word ppage = m_freelist.get_free_block(1, fbl_exception);
				if (fbl_exception.type != FreeBlockList::Exception::Type::AOK)
				{
					ERROR("Failed to retrieve physical page from free list.");
				}

				map_vpage_to_ppage(vpage, ppage, exception);
			}

			DEBUG_SS(std::stringstream() << "accessing virtual page " << std::to_string(vpage)
					<< " (maps to " << std::to_string(entry->ppage) << ")" << " of process "
					<< std::to_string(m_cur_ptable->pid));

			return entry->ppage;
		}
};

#endif /* VIRTUAL_MEMORY_H */