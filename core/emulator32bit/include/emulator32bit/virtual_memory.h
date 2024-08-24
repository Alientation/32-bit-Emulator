#pragma once
#ifndef VIRTUAL_MEMORY_H
#define VIRTUAL_MEMORY_H

#define AEMU_ONLY_CRITICAL_LOG

#include "emulator32bit/emulator32bit_util.h"
#include "emulator32bit/disk.h"
#include "emulator32bit/fbl.h"
#include "util/loggerv2.h"

#include <unordered_map>

#define VM_MAX_PAGES 1024
#define TLB_PSIZE 12
#define TLB_SIZE (1 << TLB_PSIZE)
#define MAX_PROCESSES 1024
#define NUM_PPAGES (1 << (8 * sizeof(word) - PAGE_PSIZE))

/*
	idea

	todo
	mark physical pages as swappable/non swappable so memory mapped i/o can work (and kernel specific memory can work)
		-> also allow these pages to be marked as kernel/user
	mark virtual pages with read/write/execute permissions
	fix the current way multiple virtual addresses can map to the same physical address, instead,
	check the physical page if it is swappable, if not, then that means multiple virtual addresses
	will map to the same physical address. otherwise the physical page will be swapped out.

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

class VirtualMemory
{
	public:
		VirtualMemory(Disk& disk);
		virtual ~VirtualMemory();

		Disk& m_disk;
		bool enabled = true;				/* Whether addresses should be mapped. */

		class VirtualMemoryException : public std::exception
		{
			protected:
				std::string message;

			public:
				VirtualMemoryException(const std::string& msg);

				const char* what() const noexcept override;
		};

		class InvalidPIDException : public VirtualMemoryException
		{
			protected:
				long long invalid_pid;

			public:
				InvalidPIDException(const std::string& msg, long long invalid_pid);

				long long get_invalid_pid() const noexcept;
		};

		class VPageRemapException : public VirtualMemoryException
		{
			protected:
				word vpage;
				word already_mapped_ppage;
				word attempted_mapped_ppage;

			public:
				VPageRemapException(const std::string& msg, word vpage,
						word already_mapped_ppage, word attempted_mapped_ppage);

				word get_vpage() const noexcept;
				word get_already_mapped_ppage() const noexcept;
				word get_attempted_mapped_ppage() const noexcept;
		};

		class InvalidVPageException : public VirtualMemoryException
		{
			protected:
				word vpage;

			public:
				InvalidVPageException(const std::string& msg, word vpage);

				word get_vpage() const noexcept;
		};

		/**
		 * @brief 			Represents recoverable exception states that should be handled by the
		 * 					caller.
		 *
		 * @note 			INVALID_ADDRESS and DISK_FETCH_FAILED should be instead through as a
		 * 					c++ exception. These should not be recovered from and the running user
		 * 					program should exit (or the kernel should just crash???).
		 */
		struct Exception
		{
			enum class Type
			{
				AOK,
				INVALID_ADDRESS,
				DISK_FETCH_SUCCESS,
				DISK_RETURN_AND_FETCH_SUCCESS,
				DISK_FETCH_FAILED,
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
			word ppage_fetch;						/* physical page to write disk fetch results to. */
			word ppage_return;						/* physical page to read from and write to disk at disk_page_return. */
			word disk_page_return;					/* disk page to write the read physical page to. */
		};

		/**
		 * @brief 			Sets the current proccess to change the virtual space mappings.
		 *
		 * @throws			InvalidPIDException when the pid is not a valid process.
		 * @param pid 		Process id.
		 */
		void set_process(long long pid);

		/**
		 * @brief 			Starts a new process with it's own virtual memory address space.
		 *
		 * @throws 			VirtualMemoryException when MAX_PROCESSES limit is reached.
		 * @param			kernel_privilege: Whether the process has the kernel level access
		 * 					privilege
		 * @return			New process id.
		 */
		long long begin_process(bool kernel_privilege = false);

		/**
		 * @brief 			Ends a specified process.
		 *
		 * @throws 			InvalidPIDException when the pid is not a valid process.
		 * @param pid		Process id.
		 */
		void end_process(long long pid);

		/**
		 * @brief 			Gets the current process identifier.
		 *
		 * @return 			Current process ID, -1 if no current active process.
		 */
		long long current_process();

		/**
		 * @brief 			Set the the access permissions of physical memory. Used by the kernel
		 * 					to set up memory mapped regions for I/O.
		 *
		 * @param 			ppage_begin: First physical page to set the permissions to.
		 * @param 			ppage_end: Last physical page to set the permissions to.
		 * @param 			swappable: Whether the pages in this region should be swappable.
		 * @param 			kernel_locked: Whether the pages in this region require kernel level
		 * 					privilege to access.
		 */
		void set_ppage_permissions(word ppage_begin, word ppage_end, word swappable,
								   word kernel_locked);

		/**
		 * @brief 			Set the access permissions of virtual memory specific to a process.
		 *
		 * @throws			InvalidPIDException if the pid is invalid.
		 * @param 			pid: Process to set the access permissions.
		 * @param 			vpage_begin: First virtual page to update permissions.
		 * @param 			vpage_end: Last virtual page to update permissions.
		 * @param 			write: Virtual page write permissions.
		 * @param 			execute: Virtual page execute permissions.
		 */
		void set_vpage_permissions(long long pid, word vpage_begin, word vpage_end, bool write, bool execute);

		/**
		 * @brief 			Checks the write permissions of the virtual page by the process.
		 *
		 * @throw			InvalidPIDException when pid is invalid.
		 * @param 			pid: Process identifier.
		 * @param 			vpage: Virtual page to check.
		 * @return 			Whether the virtual page can be written to.
		 */
		bool can_write_vpage(long long pid, word vpage);

		/**
		 * @brief			Checks the execute permissions of the virtual page by the process.
		 *
		 * @throw			InvalidPIDException when pid is invalid.
		 * @param 			pid: Process identifier.
		 * @param 			vpage: Virtual page to check.
		 * @return 			Whether code in the virtual page can be executed.
		 */
		bool can_execute_vpage(long long pid, word vpage);

		/**
		 * @brief			Checks the access permissions of the physical page by the process.
		 *
		 * @throw			InvalidPIDException when pid is invalid.
		 * @param 			pid: Process identifier.
		 * @param 			ppage: Physical page to check.
		 * @return 			Whether the physical page can be accessed by a process.
		 */
		bool can_access_ppage(long long pid, word ppage);

		/**
		 * @brief			Adds a new virtual page to the specified process.
		 *
		 * @throws			InvalidPIDException when pid is invalid.
		 * @throws			InvalidVPageException when the virtual page has already been mapped to
		 * 					the process.
		 * @param 			pid: ID of the process to add a virtual page to.
		 * @param 			vpage: Virtual page to add.
		 */
		void add_vpage(long long pid, word vpage, word length, bool write, bool execute);

		/**
		 * @brief 			Converts a virtual address into a physical address of the process
		 * 					specified by the process id if virtual memory
		 * 					is enabled, otherwise the virtual address is equivalent to the physical
		 * 					address.
		 *
		 * @param 			pid: ID of the process to translate virtual address.
		 * @param 			address: Virtual address to translate.
		 * @param 			exception: Exception is thrown whenever a page fault should be handled.
		 * @return 			Physical address corresponding to the virtual address.
		 */
		inline word translate_address(long long pid, word address, Exception& exception)
		{
			if (UNLIKELY(!enabled))
			{
				return address;
			}

			if (UNLIKELY(m_process_ptable_map.find(pid) == m_process_ptable_map.end()))
			{
				ERROR_SS(std::stringstream() << "Invalid Process ID " << std::to_string(pid));
			}

			return translate_address(m_process_ptable_map.at(pid), address, exception);
		}

		/**
		 * @brief 			Converts a virtual address into a physical address if virtual memory
		 * 					is enabled, otherwise the virtual address is equivalent to the physical
		 * 					address.
		 *
		 * @param 			address: Virtual address to translate.
		 * @param 			exception: Exception is thrown whenever a page fault should be handled.
		 * @return 			Physical address corresponding to the virtual address.
		 */
		inline word translate_address(word address, Exception& exception)
		{
			if (UNLIKELY(m_cur_ptable == nullptr || !enabled))
			{
				return address;
			}

			return translate_address(m_cur_ptable, address, exception);
		}

		/**
		 * @brief 			Makes a force mapping of the virtual page to the physical page if it
		 * 					is not yet mapped.
		 *
		 * @throws			InvalidPIDException if pid is invalid.
		 * @param 			pid: Process identifier.
		 * @param 			vpage: Virtual page to force map to physical page if not yet mapped.
		 * @param 			ppage: Physical page to force map to.
		 * @param 			exception: Exception raised when page faults should be handled.
		 */
		void ensure_physical_page_mapping(long long pid, word vpage, word ppage,
										  Exception& exception);


	private:
		/**
		 * @brief			Contains information about a physical page and what virtual pages
		 * 					map to it.
		 */
		struct PageTableEntry
		{
			/**
			 * @brief 		Construct a new Page Table Entry object.
			 *
			 * @param		pid: Process ID.
			 * @param 		vpage: Virtual page of this mapping.
			 * @param 		diskpage: Disk page where the virtual page resides.
			 * @param		write: Whether virtual page can be written to.
			 * @param		execute: Whether code can be executed from the virtual page.
			 */
			PageTableEntry(long long pid, word vpage, word diskpage, bool write, bool execute);

			long long pid;					/* Process that has this mapping. */
			word vpage;						/* Virtual page. */
			word ppage;						/* Mapped physical page if not on disk. */
			bool disk;						/* Whether the virtual page is on disk. */
			word diskpage;					/* Corresponding disk page where the virtual page resides. */
			bool mapped;					/* Whether this is a mapped virtual page with a permanent physical page. */
			word mapped_ppage;				/* Corresponding physical page as mentioned above. */

			bool write;						/* Whether this virtual page can be written to. */
			bool execute;					/* Whether this virtual page contains code to execute. */
		};

		struct PhysicalPage
		{
			PhysicalPage();

			std::vector<PageTableEntry*> mapped_vpages;
			word ppage;

			bool used;

			bool swappable;					/* Whether this physical page can be evicted/swapped. */
			bool kernel_locked;				/* Whether this physical page requires kernel level permission to access. */
		};

		/**
		 * @brief			Contains information about the memory mapping of a specific process.
		 */
		struct PageTable
		{
			long long pid = 0;				/* Process ID. */

			/* Mapping of virtual page address to the corresponding PageTableEntry. */
			std::unordered_map<word, PageTableEntry*> entries = std::unordered_map<word,PageTableEntry*>();

			bool kernel_privilege;
		};

		/**
		 * @brief			TBL Entry.
		 */
		struct TLB_Entry
		{
			bool valid = false;			/* Whether the TLB_Entry is a valid translation. */
			long long pid = -1;			/* Corresponding process of the translation. */
			word vpage;					/* Virtual page address of the translation. */
			word ppage;					/* Resulting physical page address of the translation. */
		};

		/**
		 * @brief 			Translation Lookaside Buffer. Contains the recently translated virtual
		 * 					page address to physical page address.
		 * @note			Keys are the hash of the virtual page address.
		 * @todo			Change so that the hash is of the virtual page address and the pid to
		 * 					avoid collisions between processes.
		 */
		TLB_Entry tlb[TLB_SIZE];

		/**
		 * @brief			Free PIDs not in use by any process.
		 */
		FreeBlockList m_freepids;

		/**
		 * @brief			Map of PID to the corresponding page table of the process.
		 */
		std::unordered_map<long long, PageTable*> m_process_ptable_map;

		/**
		 * @brief  			Map of physical pages to the corresponding PageTableEntry.
		 */
		// std::unordered_map<word, PhysicalPage*> m_physical_memory_map;
		PhysicalPage* m_physical_memory_map = new PhysicalPage[NUM_PPAGES];

		/**
		 * @brief			Free physical pages that new virtual pages can map to.
		 */
		FreeBlockList m_freelist;

		/**
		 * @brief 			Current active process in which all calls to the virtual memory to
		 * 					manipulate/use mappings without a supplied PID refers to.
		 */
		PageTable *m_cur_ptable = nullptr;

		/**
		 * @brief 			LRU_Node keeps track of the corresponding physical page and maintains
		 * 					linkages to the next and previous nodes in the LRU list.
		 */
		struct LRU_Node
		{
			word ppage;				/* Corresponding physical page. */
			LRU_Node* next;			/* Next node in the list. */
			LRU_Node* prev;			/* Previous node in the list. */
		};

		/**
		 * @brief			Beginning of the LRU list. Points to the least recently used physical
		 * 					page.
		 *
		 */
		LRU_Node* m_lru_head = nullptr;

		/**
		 * @brief			End of the LRU list. Points to the most recently used physical page.
		 *
		 */
		LRU_Node* m_lru_tail = nullptr;

		/**
		 * @brief			Maps a physical page address to the LRU_Node corresponding to it in the
		 * 					LRU list.
		 */
		std::unordered_map<word,LRU_Node*> m_lru_map;

		/**
		 * @brief 			Ensures that the virtual memory page tables memory mappings are valid.
		 */
		void check_vm();

		/**
		 * @brief 			Adds a physical page that was just used to the list.
		 *
		 * @param 			ppage: Physical page address.
		 */
		void add_lru(word ppage);

		/**
		 * @brief 			Removes the least recently used physical page from the list.
		 *
		 * @return 			Physical page address that was least recently used.
		 */
		word remove_lru();

		/**
		 * @brief			Ensures the LRU (least recently used) of the in use physical pages
		 * 					are valid.
		 */
		void check_lru();

		/**
		 * @brief 			Removes the physical page and writes it back to disk, freeing up a
		 * 					location for another virtual page to map to.
		 *
		 * @param 			ppage: Physical page to evict.
		 * @param 			exception: Exception is thrown since this is a page fault that must be
		 * 					handled by the caller.
		 */
		void evict_ppage(word ppage, Exception& exception);

		/**
		 * @brief 			Maps a virtual page to a specific physical page of the process
		 * 					corresponding to the given pid.
		 *
		 * @throw 			InvalidPIDException is pid is invalid.
		 * @param 			pid: ID of the process to map a virtual page.
		 * @param 			vpage: Virtual page to map.
		 * @param 			ppage: Physical page to map to.
		 * @param 			exception: Exception is thrown whenever there is a page fault to handle.
		 */
		void map_vpage_to_ppage(long long pid, word vpage, word ppage, Exception& exception);

		/**
		 * @brief 			Maps a new virtual page to a physical page of the specified process.
		 * 					Note this forces the virtual page to always map to the physical page.
		 *
		 * @throws			InvalidPIDException if pid is invalid.
		 * @throws 			InvalidVPageException if virtual page has already been added.
		 * @param 			pid: Process id to map virtual page.
		 * @param 			vpage: Virtual page to map.
		 * @param 			ppage: Physical page to map to.
		 * @param 			exception: Exception is thrown whenever there is a page fault to handle.
		 */
		void map_ppage(long long pid, word vpage, word ppage, Exception& exception);

		/**
		 * @brief 			Removes the virtual page from a process referenced by it's pid.
		 *
		 * @throws			InvalidPIDException if pid is invalid.
		 * @throws 			InvalidVPageException if virtual page is not mapped to process.
		 * @param 			pid: Process id.
		 * @param 			vpage: Virtual page to remove.
		 */
		void remove_vpage(long long pid, word vpage);

		/**
		 * @brief			Translates a virtual space address to a physical space address. Note these
		 * 					are not page addresses, but full memory address in the 0 to 2^31 - 1
		 * 					range.
		 *
		 * @param 			ptable: Page table of the process to access.
		 * @param 			address: Virtual space address.
		 * @param 			exception: Exception is thrown whenever there is a page fault to handle.
		 * @return 			Physical space address corresponding to the virtual space address of
		 * 					this process.
		 */
		inline word translate_address(PageTable *ptable, word address, Exception& exception)
		{
			DEBUG_SS(std::stringstream() << "Mapping address " << std::to_string(address) << ".");

			word vpage = address >> PAGE_PSIZE;
			word ppage = access_vpage(ptable, vpage, exception);

			DEBUG_SS(std::stringstream() << "Accessing virtual memory page " << std::to_string(vpage)
					<< " which is physical page " << std::to_string(ppage) << ".");

			return (ppage << PAGE_PSIZE) + (address & (PAGE_SIZE-1));
		}

		/**
		 * @brief 			Accesses a virtual page, performing the translation to the physical page
		 * 					according to the page table.
		 *
		 * @param 			ptable: Page table of the process containing the virtual address mappings.
		 * @param 			vpage: Virtual page to map.
		 * @param 			exception: Exception is thrown whenever there is a page fault to handle.
		 * @return 			Physical page address.
		 */
		inline word access_vpage(PageTable *ptable, word vpage, Exception& exception)
		{
			// check_vm();

			word tlb_addr = vpage & (TLB_SIZE-1);

			/*
			 * Unlikely that the virtual page has not been accessed recently.
			 *
			 * The TLB (Translation Lookaside Buffer) helps avoid expensive calls to the entry
			 * mapping. Recently accessed virtual pages will have the translation stored in the
			 * buffer.
			 */
			if (UNLIKELY(!tlb[tlb_addr].valid || tlb[tlb_addr].pid != ptable->pid || tlb[tlb_addr].vpage != vpage))
			{
				/*
				 * Unlikely that the virtual page accesses is an unmapped virtual page.
				 */
				if (UNLIKELY(ptable->entries.find(vpage) == ptable->entries.end()))
				{
					throw VirtualMemoryException("SIGSEGV");
				}
				else if (!ptable->entries.at(vpage)->disk)
				{
					/*
					 * Update the TLB with the result of the translation of virtual page to
					 * physical page.
					 */
					tlb[tlb_addr].valid = true;
					tlb[tlb_addr].pid = ptable->pid;
					tlb[tlb_addr].vpage = vpage;
					tlb[tlb_addr].ppage = ptable->entries.at(vpage)->ppage;
				}
			}
			else
			{
				return tlb[tlb_addr].ppage;			// translation exists in the buffer.
			}

			PageTableEntry *entry = ptable->entries.at(vpage);

			/*
			 * Likely that the virtual page being accessed has not been evicted to the disk.
			 */
			if (LIKELY(!entry->disk))
			{
				DEBUG_SS(std::stringstream() << "accessing virtual page (NOT ON DISK) "
						<< std::to_string(vpage) << " (maps to " << std::to_string(entry->ppage) << ")"
						<< " of process " << std::to_string(ptable->pid));
				return entry->ppage;
			}

			DEBUG("BRINGING PAGE ONTO RAM");

			/*
			 * Unlikely that the virtual page has been forcibly mapped to a physical page.
			 *
			 * Maintains any explicit mappings of virtual page to physical page, like
			 * writing/reading from memory mapped I/O or ports.
			 */
			if (UNLIKELY(entry->mapped))
			{
				/*
				 * Since the virtual page is mapped to a physical page on disk, we can assume it was
				 * evicted and some other page is in use at the spot.
				 */
				if (LIKELY(m_physical_memory_map[entry->mapped_ppage].used))
				{
					evict_ppage(entry->mapped_ppage, exception);
				}

				map_vpage_to_ppage(ptable->pid, vpage, entry->mapped_ppage, exception);
			}
			else
			{
				/*
				 * Unlikely that all physical pages are in use.
				 */
				if (UNLIKELY(!m_freelist.can_fit(1)))
				{
					evict_ppage(remove_lru(), exception);
				}

				word ppage = m_freelist.get_free_block(1);
				map_vpage_to_ppage(ptable->pid, vpage, ppage, exception);
			}

			DEBUG_SS(std::stringstream() << "accessing virtual page " << std::to_string(vpage)
					<< " (maps to " << std::to_string(entry->ppage) << ")" << " of process "
					<< std::to_string(ptable->pid));

			return entry->ppage;
		}

		/**
		 * @brief 			Accesses a virtual page of a specific process.
		 *
		 * @param 			pid: ID of the process to access.
		 * @param 			vpage: Virtual page address to access.
		 * @param 			exception: Exception is thrown whenever there is a page fault to handle.
		 * @return 			Physical page address.
		 */
		inline word access_vpage(long long pid, word vpage, Exception& exception)
		{
			if (UNLIKELY(m_process_ptable_map.find(pid) == m_process_ptable_map.end()))
			{
				ERROR_SS(std::stringstream() << "Invalid Process ID " << std::to_string(pid));
			}

			return access_vpage(m_process_ptable_map.at(pid), vpage, exception);
		}

		/**
		 * @brief 			Accesses a virtual page of the currently active process.
		 *
		 * @param 			vpage: Virtual page address to access.
		 * @param 			exception: Exception is thrown whenever there is a page fault to handle.
		 * @return 			Physical page address.
		 */
		inline word access_vpage(word vpage, Exception& exception)
		{
			if (UNLIKELY(m_cur_ptable == nullptr || !enabled))
			{
				return vpage;
			}

			return access_vpage(m_cur_ptable, vpage, exception);
		}
};


#endif /* VIRTUAL_MEMORY_H */