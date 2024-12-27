#include "emulator32bit/virtual_memory.h"

#include <unordered_set>

VirtualMemory::VirtualMemory(Disk *disk) :
	m_disk(disk),
	m_freepids(0, MAX_PROCESSES),
	m_freelist(0, NUM_PPAGES)
{
	for (int i = 0; i < NUM_PPAGES; i++)
	{
		m_physical_memory_map[i].ppage = (word) i;
	}
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

VirtualMemory::InvalidPIDException::InvalidPIDException(const std::string& msg, long long invalid_pid) :
	VirtualMemoryException(msg),
	invalid_pid(invalid_pid)
{

}

long long VirtualMemory::InvalidPIDException::get_invalid_pid() const noexcept
{
	return invalid_pid;
}

VirtualMemory::VPageRemapException::VPageRemapException(const std::string& msg, word vpage,
		word already_mapped_ppage, word attempted_mapped_ppage) :
	VirtualMemoryException(msg),
	vpage(vpage),
	already_mapped_ppage(already_mapped_ppage),
	attempted_mapped_ppage(attempted_mapped_ppage)
{

}

word VirtualMemory::VPageRemapException::get_vpage() const noexcept
{
	return vpage;
}

word VirtualMemory::VPageRemapException::get_already_mapped_ppage() const noexcept
{
	return already_mapped_ppage;
}

word VirtualMemory::VPageRemapException::get_attempted_mapped_ppage() const noexcept
{
	return attempted_mapped_ppage;
}

VirtualMemory::InvalidVPageException::InvalidVPageException(const std::string& msg, word vpage) :
	VirtualMemoryException(msg),
	vpage(vpage)
{

}

word VirtualMemory::InvalidVPageException::get_vpage() const noexcept
{
	return vpage;
}



VirtualMemory::PageTableEntry::PageTableEntry(long long pid, word vpage, word diskpage,
											  bool write, bool execute) :
	pid(pid),
	vpage(vpage),
	ppage(0),
	disk(true),
	diskpage(diskpage),
	mapped(false),
	mapped_ppage(0),
	write(write),
	execute(execute)
{

}

VirtualMemory::PhysicalPage::PhysicalPage() :
	mapped_vpages(std::vector<PageTableEntry*>()),
	ppage(0),
	used(false),
	swappable(true),
	kernel_locked(false)
{

}

void VirtualMemory::set_process(long long pid)
{
	if (m_process_ptable_map.find(pid) == m_process_ptable_map.end())
	{
		throw InvalidPIDException("Cannot set memory map of process " + std::to_string(pid) +
				" because it doesn't exist.", pid);
		return;
	}

	m_cur_ptable = m_process_ptable_map.at(pid);
	DEBUG("Setting memory map to process %llu.", pid);
}

long long VirtualMemory::begin_process(bool kernel_privilege)
{
	if (!m_freepids.can_fit(1))
	{
		throw VirtualMemoryException("Reached the MAX_PROCESSES limit. Cannot create a new process.");
		return -1;
	}

	word pid = m_freepids.get_free_block(1);

	PageTable *new_pagetable = new PageTable
	{
		.pid = pid,
		.kernel_privilege = kernel_privilege,
	};

	m_process_ptable_map.insert(std::make_pair(pid, new_pagetable));
	m_cur_ptable = new_pagetable;

	DEBUG("Beginning process %llu.", pid);
	return pid;
}

void VirtualMemory::end_process(long long pid)
{
	if (m_process_ptable_map.find(pid) == m_process_ptable_map.end())
	{
		throw InvalidPIDException("Cannot end process " + std::to_string(pid) + " since it does "
				"not exist.", pid);
		return;
	}

	/*
	 * Store into a temporary array because when vpage entry is removed, it will also be removed
	 * from the process PageTableEntry map. This would have caused a concurrent modification error.
	 */
	std::vector<word> vpages;
	for (std::pair<const word, PageTableEntry*>& pair : m_process_ptable_map.at(pid)->entries)
	{
		vpages.push_back(pair.first);
	}

	for (word vpage : vpages)
	{
		remove_vpage(pid, vpage);
	}

	if (m_cur_ptable == m_process_ptable_map.at(pid))
	{
		m_cur_ptable = nullptr;
	}

	delete m_process_ptable_map.at(pid);
	m_process_ptable_map.erase(pid);
	m_freepids.return_block(pid, 1);
	DEBUG("Ending process %llu.", pid);
}

long long VirtualMemory::current_process()
{
	if (m_cur_ptable == nullptr)
	{
		return -1;
	}

	return m_cur_ptable->pid;
}

void VirtualMemory::set_ppage_permissions(word ppage_begin, word ppage_end, word swappable, word kernel_locked)
{
	for (word i = ppage_begin; i <= ppage_end; i++)
	{
		m_physical_memory_map[i].swappable = swappable;
		m_physical_memory_map[i].kernel_locked = kernel_locked;
	}
}

void VirtualMemory::set_vpage_permissions(long long pid, word vpage_begin, word vpage_end, bool write, bool execute)
{
	if (UNLIKELY(m_process_ptable_map.find(pid) == m_process_ptable_map.end()))
	{
		throw InvalidPIDException("Cannot set vpage permissions since pid is invalid.", pid);
	}

	PageTable *ptable = m_process_ptable_map.at(pid);
	for (word vpage = vpage_begin; vpage <= vpage_end; vpage++)
	{
		if (ptable->entries.find(vpage) == ptable->entries.end())
		{
			add_vpage(pid, vpage, 1, write, execute);
		}
		else
		{
			PageTableEntry *entry = ptable->entries.at(vpage);
			entry->write = write;
			entry->execute = execute;
		}
	}
}

bool VirtualMemory::can_write_vpage(long long pid, word vpage)
{
	if (UNLIKELY(m_process_ptable_map.find(pid) == m_process_ptable_map.end()))
	{
		throw InvalidPIDException("Cannot check write permission of virtual page because pid is invalid.", pid);
	}

	PageTable *ptable = m_process_ptable_map.at(pid);
	if (ptable->entries.find(vpage) == ptable->entries.end())
	{
		return false;
	}
	return ptable->entries.at(vpage)->write;
}

bool VirtualMemory::can_execute_vpage(long long pid, word vpage)
{
	if (UNLIKELY(m_process_ptable_map.find(pid) == m_process_ptable_map.end()))
	{
		throw InvalidPIDException("Cannot check execute permission of virtual page because pid is invalid.", pid);
	}

	PageTable *ptable = m_process_ptable_map.at(pid);
	if (ptable->entries.find(vpage) == ptable->entries.end())
	{
		return false;
	}
	return ptable->entries.at(vpage)->execute;
}

bool VirtualMemory::can_access_ppage(long long pid, word ppage)
{
	if (UNLIKELY(m_process_ptable_map.find(pid) == m_process_ptable_map.end()))
	{
		throw InvalidPIDException("Cannot check access permission of physical page because pid is invalid.", pid);
	}

	PageTable *ptable = m_process_ptable_map.at(pid);
	return !m_physical_memory_map[ppage].kernel_locked || ptable->kernel_privilege;
}

void VirtualMemory::add_vpage(long long pid, word vpage, word length, bool write, bool execute)
{
	if (UNLIKELY(m_process_ptable_map.find(pid) == m_process_ptable_map.end()))
	{
		throw InvalidPIDException("Cannot add virtual pages because pid is invalid.", pid);
	}

	DEBUG("Adding vpages from %u to %u.", vpage, vpage + length - 1);

	PageTable *ptable = m_process_ptable_map.at(pid);
	word last_vpage = vpage + length - 1;
	for (; vpage <= last_vpage; vpage++)
	{
		if (ptable->entries.find(vpage) != ptable->entries.end())
		{
			throw InvalidVPageException("Cannot add virtual page " + std::to_string(vpage) +
					" because it is already mapped to process " + std::to_string(pid), vpage);
			return;
		}

		ptable->entries.insert(std::make_pair(vpage, new PageTableEntry(pid, vpage, m_disk->get_free_page(), write, execute)));

		DEBUG("Adding virtual page %u to process %llu.", vpage, pid);
	}
}

void VirtualMemory::map_ppage(long long pid, word vpage, word ppage, Exception& exception)
{
	if (UNLIKELY(m_process_ptable_map.find(pid) == m_process_ptable_map.end()))
	{
		throw InvalidPIDException("Cannot map virtual page to physical page because pid is invalid.", pid);
	}

	PageTable *ptable = m_process_ptable_map.at(pid);
	if (ptable->entries.find(vpage) != ptable->entries.end())
	{
		throw InvalidVPageException("Cannot map virtual page to physical page because virtual page has already been added.", vpage);
	}

	add_vpage(vpage, 1, true, true, true);

	if (m_physical_memory_map[ppage].used)
	{
		evict_ppage(ppage, exception);
	}

	m_freelist.remove_block(ppage, 1);
	map_vpage_to_ppage(pid, vpage, ppage, exception);

	PageTableEntry *entry = ptable->entries.at(vpage);
	entry->mapped = true;
	entry->mapped_ppage = ppage;
}

void VirtualMemory::remove_vpage(long long pid, word vpage)
{
	if (UNLIKELY(m_process_ptable_map.find(pid) == m_process_ptable_map.end()))
	{
		throw InvalidPIDException("Cannot remove virtual page because pid is invalid.", pid);
	}

	PageTable *ptable = m_process_ptable_map.at(pid);

	if (ptable->entries.find(vpage) == ptable->entries.end())
	{
		throw InvalidVPageException("Cannot remove virtual page because it is not mapped to process.", vpage);
		return;
	}

	PageTableEntry *entry = ptable->entries.at(vpage);
	ptable->entries.erase(vpage);

	if (entry->disk)
	{
		m_disk->return_page(entry->diskpage);

		DEBUG("Returning disk page %u coressponding to virtual page %u.", entry->diskpage, vpage);
	}
	else
	{
		m_physical_memory_map[entry->ppage].used = false;

		/* add back to free list */
		m_freelist.return_block(entry->ppage, 1);

		DEBUG("Returning physical page %u corresponding to virtual page %u.", entry->ppage, vpage);
	}

	delete entry;
}

void VirtualMemory::check_vm()
{
	for (int i = 0; i < NUM_PPAGES; i++)
	{
		PhysicalPage& ppage = m_physical_memory_map[i];

		EXPECT_TRUE((word) i == ppage.ppage, "Expected physical memory to match");

		if (ppage.mapped_vpages.size() > 0)
		{
			word diskpage = ppage.mapped_vpages.at(0)->diskpage;
			for (PageTableEntry *entry : ppage.mapped_vpages)
			{
				EXPECT_TRUE(entry->diskpage == diskpage, "Expected all virtual pages mapped to the "
						"physical page to have same diskpage location.");
			}
		}
	}

	for (std::pair<long long, PageTable*> pair : m_process_ptable_map)
	{
		DEBUG("Checking process %llu.", pair.first);
		EXPECT_TRUE(pair.second->pid == pair.first, "Expected Process ID to match");
		for (std::pair<word,PageTableEntry*> entry : pair.second->entries)
		{
			DEBUG("Checking page entry at vpage %u.", entry.first);

			EXPECT_TRUE(entry.second->vpage == entry.first, "Expected virtual memory to match");
		}
	}
}

void VirtualMemory::evict_ppage(word ppage, Exception& exception)
{
	DEBUG("Evicting physical page %u to disk.", ppage);

	/*
	 * NOTE: this location will be overwritten below since we return the
	 * block to the free list, and then request a free block immediately
	 */
	PhysicalPage& evicted_ppage = m_physical_memory_map[ppage];
	evicted_ppage.used = false;

	for (PageTableEntry *removed_entry : evicted_ppage.mapped_vpages)
	{
		removed_entry->disk = true;
		removed_entry->diskpage = m_disk->get_free_page();
		word tlb_addr = removed_entry->vpage & (TLB_SIZE-1);
		TLB_Entry& tlb_entry = tlb[tlb_addr];
		if (tlb_entry.valid && tlb_entry.ppage == ppage && tlb_entry.vpage == removed_entry->vpage) // todo, this should check for pid i think.
		{
			tlb_entry.valid = false;
		}
	}
	evicted_ppage.mapped_vpages.clear();

	// exception to tell system bus to write to disk
	exception.disk_page_return = evicted_ppage.mapped_vpages.at(0)->diskpage;		// there MUST be a mapped vpage or else we should not be evicting this ppage.
	exception.ppage_return = ppage;
	exception.type = Exception::Type::DISK_RETURN_AND_FETCH_SUCCESS;

	m_freelist.return_block(ppage, 1);
}

void VirtualMemory::map_vpage_to_ppage(long long pid, word vpage, word ppage, Exception& exception)
{
	if (UNLIKELY(m_process_ptable_map.find(pid) == m_process_ptable_map.end()))
	{
		throw InvalidPIDException("Cannot map virtual page to physical page because pid is invalid.", pid);
	}

	PageTable *ptable = m_process_ptable_map.at(pid);

	PageTableEntry *entry = ptable->entries.at(vpage);
	exception.disk_fetch = m_disk->read_page(entry->diskpage);

	DEBUG("Disk Fetch from page %u to physical page %u.", entry->diskpage, ppage);

	m_disk->return_page(entry->diskpage);

	if (exception.type != Exception::Type::DISK_RETURN_AND_FETCH_SUCCESS)
	{
		exception.type = Exception::Type::DISK_FETCH_SUCCESS;
	}
	exception.ppage_fetch = ppage;
	entry->ppage = ppage;
	entry->disk = false;

	PhysicalPage& mapped_ppage = m_physical_memory_map[ppage];
	mapped_ppage.mapped_vpages.push_back(entry);
	mapped_ppage.used = true;

	add_lru(ppage);
}

void VirtualMemory::ensure_physical_page_mapping(long long pid, word vpage, word ppage, Exception& exception)
{
	if (UNLIKELY(!enabled))
	{
		return;
	}

	if (UNLIKELY(m_process_ptable_map.find(pid) == m_process_ptable_map.end()))
	{
		throw InvalidPIDException("Invalid Process ID " + std::to_string(pid), pid);
	}

	PageTable *ptable = m_process_ptable_map.at(pid);

	/*
	 * It is likely that the virtual page has already been mapped since this is a temporary
	 * way to allow the emulator to load a program at a specific physical address.
	 */
	if (LIKELY(ptable->entries.find(vpage) != ptable->entries.end()))
	{
		/*
		 * It is likely that the virtual page maps to the same physical page.
		 */
		if (LIKELY(ptable->entries.at(vpage)->ppage == ppage))
		{
			return;
		}

		throw VPageRemapException("Virtual page " + std::to_string(vpage) + " is already "
				"mapped to a different physical page " +
				std::to_string(ptable->entries.at(vpage)->ppage) + " of process " +
				std::to_string(pid), vpage, ptable->entries.at(vpage)->ppage, ppage);
	}

	DEBUG("Mapping physical page %u to virtual page %u.", ppage, vpage);

	map_ppage(pid, vpage, ppage, exception);
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