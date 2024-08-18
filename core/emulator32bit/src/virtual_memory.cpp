#include "emulator32bit/virtual_memory.h"

#include <unordered_set>

VirtualMemory::VirtualMemory(word ram_start_page, word ram_end_page, Disk& disk) :
	m_disk(disk),
	m_ram_start_page(ram_start_page),
	m_ram_end_page(ram_end_page),
	m_freelist(ram_start_page, ram_end_page - ram_start_page + 1)
{

}

VirtualMemory::~VirtualMemory()
{
	LRU_Node *cur = m_lru_head;
	while (cur != nullptr)
	{
		LRU_Node *next = cur->next;
		delete cur;
		cur = next;
	}
}

VirtualMemory::VirtualMemoryException::VirtualMemoryException(const std::string& msg) :
	message(msg)
{

}

const char* VirtualMemory::VirtualMemoryException::what() const noexcept
{
	return message.c_str();
}

VirtualMemory::PageTableEntry::PageTableEntry(word vpage, word diskpage) :
	vpages(std::vector<word>()),
	ppage(0),
	disk(true),
	diskpage(diskpage),
	mapped(false),
	mapped_ppage(0)
{
	vpages.push_back(vpage);
}

void VirtualMemory::set_process(long long pid)
{
	if (m_process_ptable_map.find(pid) == m_process_ptable_map.end())
	{
		ERROR_SS(std::stringstream() << "Cannot set memory map of process " << std::to_string(pid)
				<< " because it doesn't exist");
		return;
	}

	m_cur_ptable = m_process_ptable_map.at(pid);
	DEBUG_SS(std::stringstream() << "Setting memory map to process " << std::to_string(pid));
}

bool VirtualMemory::is_ppage_used(word ppage)
{
	return m_physical_memory_map.find(ppage) != m_physical_memory_map.end();
}

void VirtualMemory::add_vpage(word vpage)
{
	FreeBlockList::Exception exception;

	if (m_cur_ptable == nullptr)
	{
		ERROR("Cannot add page because there is no currently running process.");
		return;
	}

	if (m_cur_ptable->entries.find(vpage) != m_cur_ptable->entries.end())
	{
		ERROR_SS(std::stringstream() << "Cannot add virtual page " << std::to_string(vpage)
				<< " because it is already mapped to process "
				<< std::to_string(m_cur_ptable->pid));
		return;
	}

	m_cur_ptable->entries.insert(std::make_pair(vpage, new PageTableEntry(vpage, m_disk.get_free_page(exception))));

	if (exception.type != FreeBlockList::Exception::Type::AOK)
	{
		ERROR("Failed to get free page from disk.");
	}

	DEBUG_SS(std::stringstream() << "Adding virtual page " << std::to_string(vpage)
			<< " to process " << std::to_string(m_cur_ptable->pid));
}

// FORCIBLY MAP A VIRTUAL PAGE TO A SPECIFIC PAGE, EVICTING WHATEVER WAS THERE PREVIOUSLY
void VirtualMemory::map_ppage(word vpage, word ppage, Exception& exception)
{
	if (m_cur_ptable == nullptr)
	{
		ERROR("Cannot map physical page because there is no currently running process.");
		return;
	}

	if (m_cur_ptable->entries.find(vpage) != m_cur_ptable->entries.end())
	{
		ERROR_SS(std::stringstream() << "Cannot map physical page " << std::to_string(ppage)
				<< " to virtual page" << std::to_string(vpage));
	}

	add_vpage(vpage);

	if (is_ppage_used(ppage))
	{
		evict_ppage(ppage, exception);
	}

	FreeBlockList::Exception fbl_exception;
	m_freelist.remove_block(ppage, 1, fbl_exception);

	if (fbl_exception.type != FreeBlockList::Exception::Type::AOK)
	{
		ERROR_SS(std::stringstream() << "Could not remove ppage " << std::to_string(ppage)
				<< " from free list.");
	}

	map_vpage_to_ppage(vpage, ppage, exception);

	PageTableEntry *entry = m_cur_ptable->entries.at(vpage);
	entry->mapped = true;
	entry->mapped_ppage = ppage;
}

// WARNING, THIS WILL NOT REMOVE THE ENTRY FROM THE PROCESS TABLE->ENTRIES MAPPING SINCE THIS
// FOR NOW IS ONLY EVER CALLED WHEN A PROCESS ENDS
void VirtualMemory::remove_vpage(long long pid, word vpage)
{
	PageTable* ptable = m_process_ptable_map.at(pid);

	if (ptable->entries.find(vpage) == ptable->entries.end())
	{
		ERROR_SS(std::stringstream() << "Cannot remove virtual page " << std::to_string(vpage)
				<< " because it is not mapped in process " << std::to_string(pid));
		return;
	}

	PageTableEntry *entry = ptable->entries.at(vpage);
	if (entry->disk)
	{
		FreeBlockList::Exception exception;
		m_disk.return_page(entry->diskpage, exception);

		if (exception.type != FreeBlockList::Exception::Type::AOK)
		{
			WARN_SS(std::stringstream() << "Failed to return virtual page " << std::to_string(vpage)
					<< " at disk page " << std::to_string(entry->diskpage) << ".");
			return;
		}

		DEBUG_SS(std::stringstream() << "Returning disk page " << std::to_string(entry->diskpage)
				<< " corresponding to virtual page " << std::to_string(vpage));
	}
	else
	{
		m_physical_memory_map.erase(entry->ppage);

		/* add back to free list */
		FreeBlockList::Exception exception;
		m_freelist.return_block(entry->ppage, 1, exception);

		if (exception.type != FreeBlockList::Exception::Type::AOK)
		{
			ERROR_SS(std::stringstream() << "Failed to return physical page "
					<< std::to_string(entry->ppage) << " to free list.");
		}

		DEBUG_SS(std::stringstream() << "Returning physical page " << std::to_string(entry->ppage)
				<< " corresponding to virtual page " << std::to_string(vpage));
	}

	delete entry;
}

void VirtualMemory::check_vm()
{
	for (std::pair<word,PageTableEntry*> pair : m_physical_memory_map)
	{
		DEBUG_SS(std::stringstream() << "Checking physical page " << std::to_string(pair.first));
		EXPECT_TRUE(pair.first == pair.second->ppage, "Expected physical memory to match");
		EXPECT_TRUE(pair.second->disk == false, "Expected physical memory to not be on disk");
	}

	for (std::pair<long long, PageTable*> pair : m_process_ptable_map)
	{
		DEBUG_SS(std::stringstream() << "Checking process " << std::to_string(pair.first));
		EXPECT_TRUE(pair.second->pid == pair.first, "Expected Process ID to match");
		for (std::pair<word,PageTableEntry*> entry : pair.second->entries)
		{
			DEBUG_SS(std::stringstream() << "Checking page entry at vpage "
					<< std::to_string(entry.first));

			word found = false;
			for (word vpage : entry.second->vpages)
			{
				if (vpage == entry.first)
				{
					found = true;
					break;
				}
			}

			EXPECT_TRUE(found, "Expected virtual memory to match");

			if (!entry.second->disk)
			{
				EXPECT_TRUE(m_physical_memory_map.at(entry.second->ppage) == entry.second,
						"Expected page entry to match.");
			}
		}
	}
}

void VirtualMemory::evict_ppage(word ppage, Exception& exception)
{
	FreeBlockList::Exception fbl_exception;

	DEBUG_SS(std::stringstream() << "Evicting physical page " << std::to_string(ppage)
			<< " to disk");

	/*
	 * NOTE: this location will be overwritten below since we return the
	 * block to the free list, and then request a free block immediately
	 */
	PageTableEntry* removed_entry = m_physical_memory_map.at(ppage);

	removed_entry->disk = true;
	removed_entry->diskpage = m_disk.get_free_page(fbl_exception);

	for (word vpage : removed_entry->vpages)
	{
		word tlb_addr = vpage & (TLB_SIZE-1);
		TLB_Entry& tlb_entry = tlb[tlb_addr];
		if (tlb_entry.valid && tlb_entry.ppage == ppage && tlb_entry.vpage == vpage)
		{
			tlb_entry.valid = false;
		}
	}

	if (fbl_exception.type != FreeBlockList::Exception::Type::AOK)
	{
		ERROR("FBL Exception");
	}

	// exception to tell system bus to write to disk
	exception.disk_page_return = removed_entry->diskpage;
	exception.ppage_return = ppage;
	exception.type = Exception::Type::DISK_RETURN_AND_FETCH_SUCCESS;

	m_freelist.return_block(ppage, 1, fbl_exception);
	if (fbl_exception.type != FreeBlockList::Exception::Type::AOK)
	{
		ERROR("FBL Exception");
	}
}

void VirtualMemory::map_vpage_to_ppage(word vpage, word ppage, Exception& exception)
{
	FreeBlockList::Exception fbl_exception;
	PageTableEntry *entry = m_cur_ptable->entries.at(vpage);
	exception.disk_fetch = m_disk.read_page(entry->diskpage);

	DEBUG_SS(std::stringstream() << "Disk Fetch from page " << std::to_string(entry->diskpage)
			<< " to physical page " << std::to_string(ppage));

	m_disk.return_page(entry->diskpage, fbl_exception);
	if (fbl_exception.type != FreeBlockList::Exception::Type::AOK)
	{
		ERROR("Failed to return disk page to free list.");
	}

	if (exception.type != Exception::Type::DISK_RETURN_AND_FETCH_SUCCESS)
	{
		exception.type = Exception::Type::DISK_FETCH_SUCCESS;
	}
	exception.ppage_fetch = ppage;
	entry->ppage = ppage;
	entry->disk = false;

	m_physical_memory_map[ppage] = entry;
	add_lru(ppage);
}

// todo these should be allocated by pages IMO
void VirtualMemory::begin_process(long long pid, word alloc_mem_begin, word alloc_mem_end)
{
	if (m_process_ptable_map.find(pid) != m_process_ptable_map.end())
	{
		ERROR("Cannot create memory map for new process because an existing process pid "
				"already exists.");
		return;
	}

	word page_begin = alloc_mem_begin >> PAGE_PSIZE;
	word page_end = alloc_mem_end >> PAGE_PSIZE;

	PageTable *new_pagetable = new PageTable
	{
		.pid = pid
	};

	m_process_ptable_map.insert(std::make_pair(pid, new_pagetable));
	m_cur_ptable = new_pagetable;

	if (alloc_mem_begin != alloc_mem_end)
	{
		for (; page_begin <= page_end; page_begin++)
		{
			add_vpage(page_begin);
		}
	}

	DEBUG_SS(std::stringstream() << "Beginning process " << std::to_string(m_cur_ptable->pid));
}

void VirtualMemory::end_process(long long pid)
{
	if (m_process_ptable_map.find(pid) == m_process_ptable_map.end())
	{
		ERROR_SS(std::stringstream() << "Cannot end process " << std::to_string(pid)
				<< "'s memory map since it does not have one.");
		return;
	}

	for (std::pair<const word, PageTableEntry*>& pair : m_process_ptable_map.at(pid)->entries)
	{
		remove_vpage(pid, pair.first);
	}

	if (m_cur_ptable == m_process_ptable_map.at(pid))
	{
		m_cur_ptable = nullptr;
	}

	delete m_process_ptable_map.at(pid);
	m_process_ptable_map.erase(pid);
	DEBUG_SS(std::stringstream() << "Ending process " << std::to_string(pid));
}

void VirtualMemory::check_lru()
{
	DEBUG("Checking LRU");

	if (m_lru_head == nullptr || m_lru_tail == nullptr)
	{
		EXPECT_TRUE(m_lru_head == m_lru_tail, "Expected list to be empty");
		EXPECT_TRUE(m_lru_map.size() == 0, "Expected lru map to be empty since list is empty");
		return;
	}

	LRU_Node *cur = m_lru_head;
	std::unordered_set<word> mapped_ppages;
	while (cur->next != nullptr)
	{
		EXPECT_TRUE(cur == cur->next->prev, "Expected the next node's previous to point back");
		EXPECT_TRUE(m_lru_map.at(cur->ppage) == cur, "Expected lru map to match list");
		mapped_ppages.insert(cur->ppage);
		cur = cur->next;
	}
	mapped_ppages.insert(cur->ppage);

	EXPECT_TRUE(cur == m_lru_tail, "Expected list to end at tail");

	for (std::pair<word,LRU_Node*> pair : m_lru_map)
	{
		EXPECT_TRUE_SS(mapped_ppages.find(pair.first) != mapped_ppages.end(), std::stringstream()
				<< "Expected LRU map to correspond to the LRU list. " << std::to_string(pair.first)
				<< " is not in the lru list");
	}
}

/* Move an lru list node respective to a physical page address back to the tail */
void VirtualMemory::add_lru(word ppage)
{
	// check_lru();

	/* this node exists in the lru list, so access it */
	if (m_lru_map.find(ppage) != m_lru_map.end())
	{
		LRU_Node *node = m_lru_map.at(ppage);

		if (node == m_lru_tail) {
			return;
		}

		/* Update new list head */
		if (node == m_lru_head)
		{
			m_lru_head = m_lru_head->next;
		}
		else
		{
			node->prev->next = node->next;
		}
		/* make the next node point back to the correct node */
		node->next->prev = node->prev;

		/* add node at the tail */
		m_lru_tail->next = node;
		node->prev = m_lru_tail;
		node->next = nullptr;
		m_lru_tail = node;

		// check_lru();
		return;
	}

	/* Empty list, just add it */
	if (m_lru_head == nullptr)
	{
		m_lru_head = new LRU_Node
		{
			.ppage = ppage,
			.next = nullptr,
			.prev = nullptr,
		};
		m_lru_tail = m_lru_head;
		m_lru_map.insert(std::make_pair(ppage, m_lru_head));

		// check_lru();
		return;
	}

	/* Add node at the end since it does not already exist in the list */
	LRU_Node *new_node = new LRU_Node
	{
		.ppage = ppage,
		.next = nullptr,
		.prev = m_lru_tail,
	};
	m_lru_tail->next = new_node;
	m_lru_tail = new_node;

	/* Update the node map to maintain O(1) */
	m_lru_map.insert(std::make_pair(ppage, m_lru_tail));

	// check_lru();
}

word VirtualMemory::remove_lru()
{
	LRU_Node *removed_node = m_lru_head;
	m_lru_head = m_lru_head->next;

	/* Checks if it was the last node meaning tail pointer has to be updated */
	if (m_lru_head == nullptr)
	{
		m_lru_tail = nullptr;
	}
	else
	{
		/* remove dangling pointer */
		m_lru_head->prev = nullptr;
	}

	word lru_ppage = removed_node->ppage;
	m_lru_map.erase(lru_ppage);
	delete removed_node;

	// check_lru();
	return lru_ppage;
}