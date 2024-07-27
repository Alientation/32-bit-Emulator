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
	FreeBlock* freeblock = find(length);

	if (!freeblock) {
		exception.type = Exception::Type::NOT_ENOUGH_SPACE;
		exception.length = length;
		// lgr::log(lgr::Logger::LogType::WARN, std::stringstream()
		//		<< "Not enough space to retrieve free block of length " << std::to_string(length));
		return 0;
	}

	word addr = freeblock->addr;
	freeblock->len -= length;
	freeblock->addr += length;

	if (freeblock->len == 0) {
		remove(freeblock);
	}

	// lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Getting free block "
	//			<< std::to_string(addr) << " to fit " << std::to_string(length));
	return addr;
}

FreeBlockList::FreeBlock* FreeBlockList::insert(word addr, word length) {
	if (!m_head || addr < m_head->addr) {
		m_head = new FreeBlock {
			.addr = addr,
			.len = length,
			.next = m_head,
		};

		// lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Initializing Free Block List");
		return m_head;
	}

	FreeBlock *cur = m_head;
	while (cur->next != nullptr && cur->next->addr < addr) {
		cur = cur->next;
	}

	FreeBlock *next = new FreeBlock {
		.addr = addr,
		.len = 1,
		.next = cur->next,
		.prev = cur,
	};

	if (cur->next) {
		cur->next->prev = next;
	}
	cur->next = next;
	return next;
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

	FreeBlock *ret_block = insert(addr, length);

	bool intersect_prev = ret_block->prev && ret_block->prev->addr + ret_block->prev->len > addr;
	bool intersect_next = ret_block->next && ret_block->next->addr < addr + length;
	if (intersect_prev || intersect_next) {
		exception.type = Exception::Type::DOUBLE_FREE;
		exception.addr = addr;
		exception.length = length;

		/* Undo state change so that the caller can cleanly handle the exception */
		remove(ret_block);
		return;
	}

	coalesce(ret_block);
	coalesce(ret_block->prev);
}

void FreeBlockList::return_all() {
	FreeBlock *cur = m_head;
	while (cur) {
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
	while (cur) {
		blocks.push_back(std::pair<word,word>(cur->addr, cur->len));
		cur = cur->next;
	}
	// lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Getting all blocks");
	return std::move(blocks);
}

void FreeBlockList::print_blocks() {
	std::vector<std::pair<word,word>> blocks = get_blocks();
	for (auto pair : blocks) {
		printf("Block {addr=%x, len=%x}\n", pair.first, pair.second);
	}
}

bool FreeBlockList::can_fit(word length) {
	return find(length) != nullptr;
}

FreeBlockList::FreeBlock* FreeBlockList::find(word length) {
	FreeBlock* cur = m_head;
	while (cur) {
		if (cur->len >= length) {
			break;
		}
		cur = cur->next;
	}
	return cur;
}

void FreeBlockList::remove(FreeBlock *node) {
	if (node->prev) {
		node->prev->next = node->next;
	} else {
		m_head = node->next;
	}

	if (node->next) {
		node->next->prev = node->prev;
	}

	delete node;
}

void FreeBlockList::coalesce(FreeBlock *first) {
	if (!first || !first->next) {
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
