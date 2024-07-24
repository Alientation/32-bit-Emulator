#include "emulator32bit/VirtualMemory.h"
#include "util/Logger.h"

VirtualMemory::VirtualMemory(word ram_start_page, word ram_end_page, Disk& disk)
		: m_disk(disk), m_ram_start_page(ram_start_page), m_ram_end_page(ram_end_page), m_freelist(ram_start_page, ram_end_page - ram_start_page + 1) {

}

void VirtualMemory::set_process(long long pid) {
	if (m_process_ptable_map.find(pid) == m_process_ptable_map.end()) {
		lgr::log(lgr::Logger::LogType::ERROR, std::stringstream()
				<< "Cannot set memory map of process " << std::to_string(pid) << " because it doesn't exist");
		return;
	}

	m_cur_ptable = m_process_ptable_map.at(pid);
}

void VirtualMemory::add_page(word vpage) {
	FreeBlockList::Exception exception;

	if (m_cur_ptable == nullptr) {
		lgr::log(lgr::Logger::LogType::ERROR, std::stringstream()
				<< "Cannot add page because there is no currently running process.");
		return;
	}

	if (m_cur_ptable->entries.find(vpage) != m_cur_ptable->entries.end()) {
		lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Cannot add virtual page "
				<< std::to_string(vpage) << " because it is already mapped to process " << std::to_string(m_cur_ptable->pid));
		return;
	}

	m_cur_ptable->entries.at(vpage) = (PageTableEntry) {
		.vpage = vpage,
		.ppage = (word) -1,
		.dirty = false,
		.disk = true,
		.diskpage = m_disk.get_free_page(exception),
	};
}

void VirtualMemory::remove_page(word vpage) {
	if (m_cur_ptable == nullptr) {
		lgr::log(lgr::Logger::LogType::ERROR, std::stringstream()
				<< "Cannot remove page because there is no currently running process.");
		return;
	}

	if (m_cur_ptable->entries.find(vpage) == m_cur_ptable->entries.end()) {
		lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Cannot remove virtual page "
				<< std::to_string(vpage) << " because it is not mapped in process " << std::to_string(m_cur_ptable->pid));
		return;
	}

	PageTableEntry& entry = m_cur_ptable->entries.at(vpage);
	if (entry.disk) {
		FreeBlockList::Exception exception;
		m_disk.return_page(entry.diskpage, exception);

		if (exception.type != FreeBlockList::Exception::Type::AOK) {
			lgr::log(lgr::Logger::LogType::WARN, std::stringstream() << "Failed to return virtual page "
					<< std::to_string(vpage) << " at disk page " << std::to_string(entry.diskpage) << ".");
			return;
		}
	} else {
		m_physical_memory_map.erase(entry.ppage);

		/* add back to free list */
		FreeBlockList::Exception exception;
		m_freelist.return_block(entry.ppage, 1, exception);

		if (exception.type != FreeBlockList::Exception::Type::AOK) {
			lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Failed to return physical page "
					<< std::to_string(entry.ppage) << " to free list.");
		}
	}
}

word VirtualMemory::access_page(word vpage) {
	if (m_cur_ptable == nullptr) {
		lgr::log(lgr::Logger::LogType::WARN, std::stringstream()
				<< "Accessing physical page instead of virtual page since there is no currently mapped process.");
		return vpage;
	}

	if (m_cur_ptable->entries.find(vpage) == m_cur_ptable->entries.end()) {
		add_page(vpage);			/* Allocate new page to this process */
	}

	PageTableEntry &entry = m_cur_ptable->entries.at(vpage);
	if (!entry.disk) {				/* Not on disk */
		return entry.ppage;
	}

	/* Bring page from disk onto RAM */
	FreeBlockList::Exception exception;
	m_freelist.get_free_block(1, exception);
	if (exception.type != FreeBlockList::Exception::Type::AOK) {
		lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Failed to retrieve physical page from free list.");
	}

	return 0;
}

void VirtualMemory::begin_process(long long pid, word alloc_mem_begin, word alloc_mem_end) {
	if (m_process_ptable_map.find(pid) != m_process_ptable_map.end()) {
		lgr::log(lgr::Logger::LogType::ERROR, std::stringstream()
				<< "Cannot create memory map for new process because an existing process pid already exists.");
		return;
	}

	word page_begin = alloc_mem_begin >> PAGE_PSIZE;
	word page_end = alloc_mem_end >> PAGE_PSIZE;

	PageTable *new_pagetable = new PageTable {
		.pid = pid
	};

	m_process_ptable_map.at(pid) = new_pagetable;
	m_cur_ptable = new_pagetable;

	for (; page_begin <= page_end; page_begin++) {
		add_page(page_begin);
	}
}

void VirtualMemory::end_process(long long pid) {
	if (m_process_ptable_map.find(pid) != m_process_ptable_map.end()) {
		lgr::log(lgr::Logger::LogType::ERROR, std::stringstream()
				<< "Cannot end process " << std::to_string(pid) << "'s memory map since it does not have one.");
		return;
	}

	if (m_cur_ptable == m_process_ptable_map.at(pid)) {
		m_cur_ptable = nullptr;
	}

	for (std::pair<const word, PageTableEntry>& pair : m_process_ptable_map.at(pid)->entries) {
		remove_page(pair.first);
	}
	delete m_process_ptable_map.at(pid);
	m_process_ptable_map.erase(pid);
}

word VirtualMemory::map_address(word address, Exception& exception) {
	if (m_cur_ptable == nullptr) {
		lgr::log(lgr::Logger::LogType::WARN, std::stringstream()
				<< "There is no associated PID to map address " << std::to_string(address) << ".");
		return address;		/* for now, just warn about it */
	}


	word vpage = address >> PAGE_PSIZE;

	return address;
}

MockVirtualMemory::MockVirtualMemory(word ram_start_page, word ram_end_page) : VirtualMemory(ram_start_page, ram_end_page, mockdisk), mockdisk() {

}

void MockVirtualMemory::set_process(long long pid) {

}

void MockVirtualMemory::begin_process(long long pid, word alloc_mem_begin, word alloc_mem_end) {

}

void MockVirtualMemory::end_process(long long pid) {

}


word MockVirtualMemory::map_address(word address, Exception& exception) {
	return address;
}