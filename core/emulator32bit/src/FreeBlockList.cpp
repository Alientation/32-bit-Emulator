#include "emulator32bit/FreeBlockList.h"

#include "util/Logger.h"

FreeBlockList::FreeBlockList(word begin, word len, bool init) : m_begin(begin), m_len(len) {
	if (init) {
		m_head = new FreeBlock{
			.addr = begin,
			.len = len,
		};
	}
	// lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Initializing Free Block List");
}

FreeBlockList::~FreeBlockList() {
	FreeBlock *cur = m_head;
	while (cur != nullptr) {
		FreeBlock *next = cur->next;
		delete(cur);
		cur = next;
	}
	// lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Destroying Free Block List");
}

word FreeBlockList::get_free_block(word length, Exception& exception) {
	FreeBlock* cur = m_head;

	while (cur != nullptr) {
		if (cur->len < length) {
			cur = cur->next;
			continue;
		}

		cur->len -= length;
		word addr = cur->addr;
		cur->addr += length;

		if (cur->len == 0) {
			if (cur == m_head) {
				FreeBlock *cur = m_head;
				m_head = m_head->next;
				delete cur;
				if (m_head) {
					m_head->prev = nullptr;
				}
			} else {
				FreeBlock *prev = cur->prev;
				prev->next = cur->next;
				delete cur;
				prev->next->prev = prev;
			}
		}

		// lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Getting free block " << std::to_string(addr)
				// << " to fit " << std::to_string(length));
		return addr;
	}

	exception.type = Exception::Type::NOT_ENOUGH_SPACE;
	exception.length = length;
	// lgr::log(lgr::Logger::LogType::WARN, std::stringstream() << "Not enough space to fit " << std::to_string(length));
	return 0;
}

void FreeBlockList::return_block(word addr, word length, Exception& exception) {
	if (addr < m_begin || addr + length > m_begin + m_len) {
		exception.type = Exception::Type::INVALID_ADDR;
		exception.addr = addr;
		exception.length = length;
		// lgr::log(lgr::Logger::LogType::WARN, std::stringstream() << "Returning block with invalid address "
				// << std::to_string(addr) << " and length " << std::to_string(length));
		return;
	}

	if (m_head == nullptr) {
		m_head = new FreeBlock {
			.addr = addr,
			.len = length,
		};
		// lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Returning block "
				// << std::to_string(addr) << " of length " << std::to_string(length) << " to empty list");
		return;
	}

	if (addr + length <= m_head->addr) {
		FreeBlock *new_head = new FreeBlock {
			.addr = addr,
			.len = length,
			.next = m_head,
		};
		m_head->prev = new_head;
		m_head = new_head;
		coalesce(m_head);
		// lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Returning block " << std::to_string(addr)
				// << " of length " << std::to_string(length) << " to beginning of list");
		return;
	}

	FreeBlock *cur = m_head;
	while (cur != nullptr) {
		if (cur->addr < addr + length && cur->addr + cur->len > addr) {
			/* overlap, error */
			exception.type = Exception::Type::DOUBLE_FREE;
			exception.addr = addr;
			exception.length = length;
			// lgr::log(lgr::Logger::LogType::WARN, std::stringstream() << "Returning block " << std::to_string(addr)
					// << " of length " << std::to_string(length) << " which overlaps free block in list.");
			return;
		}

		if (cur->next != nullptr && cur->next->addr <= addr) {
			cur = cur->next;
			continue;
		}

		FreeBlock *next = new FreeBlock {
			.addr = addr,
			.len = length,
			.next = cur->next,
			.prev = cur,
		};

		if (cur->next) {
			cur->next->prev = next;
		}
		cur->next = next;
		coalesce(cur->next);
		coalesce(cur);
		// lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Returning block " << std::to_string(addr)
				// << " of length " << std::to_string(length) << " to the list.");
		return;
	}
}

void FreeBlockList::return_all() {
	FreeBlock *cur = m_head;
	while (cur != nullptr) {
		FreeBlock *next = cur->next;
		cur = cur->next;
		delete next;
	}

	m_head = new FreeBlock {
		.addr = m_begin,
		.len = m_len,
	};
	// lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Returning all.");
}

std::vector<std::pair<word,word>> FreeBlockList::get_blocks() {
	std::vector<std::pair<word,word>> blocks;
	FreeBlock *cur = m_head;
	while (cur != nullptr) {
		blocks.push_back(std::pair<word,word>(cur->addr, cur->len));
		cur = cur->next;
	}
	// lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Getting all blocks");
	return blocks;
}

void FreeBlockList::coalesce(FreeBlock *first) {
	if (first->next == nullptr) {
		return;
	}

	if (first->next->addr != first->addr + first->len) {
		return;
	}

	first->len += first->next->len;
	FreeBlock *cur = first->next;
	first->next = first->next->next;

	if (first->next) {
		first->next->prev = first;
	}
	delete cur;
}

bool FreeBlockList::can_fit(word length) {
	FreeBlock* cur = m_head;
	while (cur != nullptr) {
		if (cur->len >= length) {
			// lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Can fit " << std::to_string(length));
			return true;
		}

		cur = cur->next;
	}
	// lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Cannot fit " << std::to_string(length));
	return false;
}