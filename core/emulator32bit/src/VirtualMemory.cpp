#include "emulator32bit/VirtualMemory.h"
#include "util/Logger.h"

VirtualMemory::VirtualMemory(Disk& disk) : m_disk(disk) {

}

void VirtualMemory::begin_process(long long pid, word alloc_mem_begin, word alloc_mem_end) {
	if (m_ptable_map.find(pid) != m_ptable_map.end()) {
		lgr::log(lgr::Logger::LogType::ERROR, std::stringstream()
				<< "Cannot create memory map for new process because an existing process pid already exists.");
		return;
	}

	word page_begin = alloc_mem_begin >> VM_PAGE_PSIZE;
	word page_end = (alloc_mem_end >> VM_PAGE_PSIZE);

	PageTable *new_pagetable = new PageTable {
		.pid = pid
	};

	for (; page_begin <= page_end; page_begin++) {
		Disk::PageManagementException exception;
		new_pagetable->entries.at(page_begin) = (PageTableEntry) {
			.vpage = page_begin,
			.ppage = 0,
			.dirty = false,
			.disk = true,
			.diskpage = m_disk.get_free_page(exception),
		};
	}

	m_ptable_map.at(pid) = new_pagetable;
	m_ptable = new_pagetable;
}

void VirtualMemory::end_process(long long pid) {
	if (m_ptable_map.find(pid) != m_ptable_map.end()) {
		lgr::log(lgr::Logger::LogType::ERROR, std::stringstream()
				<< "Cannot end process " << std::to_string(pid) << "'s memory map since it does not have one.");
		return;
	}

	if (m_ptable == m_ptable_map.at(pid)) {
		m_ptable = nullptr;
	}

	for (std::pair<const word, PageTableEntry>& pair : m_ptable_map.at(pid)->entries) {
		Disk::PageManagementException exception;
		m_disk.return_page(pair.second.diskpage, exception);
	}
	m_ptable_map.erase(pid);
}

word VirtualMemory::map_address(word address, Exception& exception) {
	return address;
}

MockVirtualMemory::MockVirtualMemory() : VirtualMemory(mockdisk), mockdisk() {

}

void MockVirtualMemory::begin_process(long long pid, word alloc_mem_begin, word alloc_mem_end) {

}

void MockVirtualMemory::end_process(long long pid) {

}


word MockVirtualMemory::map_address(word address, Exception& exception) {
	return address;
}