#include "emulator32bit/Disk.h"

#include "util/Logger.h"

Disk::Disk(File diskfile, std::streamsize npages) {
	this->m_diskfile = diskfile;
	this->m_diskfile_manager = File(diskfile.get_path() + ".info", true);
	this->m_npages = npages;
	this->m_cache = new CachePage[DISK_CACHE_SIZE];

	if (npages == 0) {
		return;
	}

	/* set up disk memory */
	std::ofstream file(m_diskfile.get_path(), std::ios::binary | std::ios::ate | std::ios::out);
    if (!file.is_open()) {
		lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Error opening disk file");
    }

	std::streamsize size = file.tellp();
	if (size == npages * DISK_PAGE_SIZE) {
		return;
	} else if (size > npages * DISK_PAGE_SIZE) {
		file.close();
		lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Disk file is larger than what is specified");
	}

	std::streamsize padding_size = npages * DISK_PAGE_SIZE - size;
	std::vector<char> padding(padding_size, 0);
	file.write(padding.data(), padding_size);
	file.close();
	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Successfully created disk file");

	/* set up disk free page management */
	FileReader freader(m_diskfile_manager, std::ios::binary | std::ios::in);

	if (!freader.has_next_byte()) {
		/* set up page managment */
		m_freehead = new FreePage();
		m_freehead->p_addr = 0;
		m_freehead->len = npages;
		m_freehead->next = nullptr;

		freader.close();
		return;
	}

	std::vector<byte> bytes;
	while (freader.has_next_byte()) {
		bytes.push_back(freader.read_byte());
	}
	ByteReader reader(bytes);

	FreePage *prev = nullptr;
	while (reader.has_next()) {
		word p_addr = reader.read_word();
		word len = reader.read_word();

		FreePage *next = new FreePage();
		next->p_addr = p_addr;
		next->len = len;
		next->next = nullptr;

		if (prev) {
			prev->next = next;
		} else {
			m_freehead = next;
		}
		prev = next;
	}
	freader.close();
}

Disk::~Disk() {
	delete[] this->m_cache;
}

word Disk::get_free_page(PageManagementException& exception) {
	if (m_freehead == nullptr) {
		exception.type = PageManagementException::Type::NOT_ENOUGH_SPACE;
		return 0;
	}

	word p_addr = m_freehead->p_addr;
	m_freehead->p_addr++;
	m_freehead->len--;

	if (m_freehead->len == 0) {
		FreePage *prev = m_freehead;
		m_freehead = m_freehead->next;
		delete prev;
	}

	return p_addr;
}

bool coalesce(Disk::FreePage *block) {
	/* Check if coalescing is necessary */
	if (block->next == nullptr || block->next->p_addr != block->p_addr + block->len) {
		return false;
	}

	block->len += block->next->len;
	Disk::FreePage *prev = block->next;
	block->next = block->next->next;
	delete prev;
	return true;
}

void Disk::return_page(word p_addr, PageManagementException& exception) {
	/* Invalid address, there is not that many pages */
	if (p_addr >= m_npages) {
		exception.p_addr = p_addr;
		exception.type = PageManagementException::Type::INVALID_PADDR;
		return;
	}

	/* If the cached block the previous freed page entered is what we want */
	if (m_prevreturn != nullptr && m_prevreturn->p_addr + m_prevreturn->len == p_addr) {
		m_prevreturn->len++;
		coalesce(m_prevreturn);
		return;
	}

	/* If the free list is empty */
	if (m_freehead == nullptr) {
		m_freehead = new FreePage();
		m_freehead->p_addr = p_addr;
		m_freehead->len = 1;
		m_freehead->next = nullptr;
		m_prevreturn = m_freehead;
		return;
	}

	/* If the freed page is before the free list */
	if (p_addr < m_freehead->p_addr) {
		FreePage *new_head = new FreePage();
		new_head->p_addr = p_addr;
		new_head->len = 1;
		new_head->next = m_freehead;
		m_freehead = new_head;
		coalesce(m_freehead);
		m_prevreturn = m_freehead;
		return;
	}

	FreePage *cur = m_freehead;
	while (cur != nullptr) {
		/* Check if the page address is already inside a free block, exception if it is */
		if (p_addr >= cur->p_addr && p_addr < cur->p_addr + cur->len) {
			exception.p_addr = p_addr;
			exception.type = PageManagementException::Type::DOUBLE_FREE;
			return;
		}

		/* If the next block page address is before the returned page, keep going */
		if (cur->next != nullptr && cur->next->p_addr <= p_addr) {	/* note, if equal, this is an error, will be caught in the next iteration in the check above */
			cur = cur->next;
			continue;
		}

		printf("at block %x, len=%x\n", cur->p_addr, cur->len);

		/* The next page  */
		FreePage *new_next = new FreePage();
		new_next->p_addr = p_addr;
		new_next->len = 1;
		new_next->next = cur->next;
		cur->next = new_next;
		m_prevreturn = cur->next;

		coalesce(cur->next);
		if (coalesce(cur)) {	/* Since the free block that the freed page was inserted into has to have been
									joined with the current block, update cached pointer */
			m_prevreturn = cur;
		}
		return;
	}

	exception.type = PageManagementException::Type::INVALID_PADDR;
	exception.p_addr = p_addr;
}

void Disk::return_all_pages() {
	FreePage *cur = m_freehead;
	while (cur != nullptr) {
		FreePage *next = cur->next;
		delete(cur);
		cur = next;
	}

	m_freehead = new FreePage();
	m_freehead->p_addr = 0;
	m_freehead->len = m_npages;
	m_freehead->next = nullptr;
}

void Disk::return_pages(word p_addr_lo, word p_addr_hi, PageManagementException& exception) {
	// todo
}

dword Disk::read_val(word address, int n_bytes, ReadException &exception) {
	address += n_bytes - 1;
	word p_addr = address >> DISK_PAGE_PSIZE;
	word offset = address & (DISK_PAGE_SIZE - 1);
	dword val = 0;
	CachePage& cpage = get_cpage(p_addr);
	for (int i = 0; i < n_bytes; i++) {
		if (offset == -1) {
			offset = DISK_PAGE_SIZE - 1;
			p_addr--;
			cpage = get_cpage(p_addr);
		}

		val <<= 8;
		val += cpage.data[offset];
		offset--;
	}
	return val;
}

byte Disk::read_byte(word address, ReadException &exception) {
	return read_val(address, 1, exception);
}
hword Disk::read_hword(word address, ReadException &exception) {
	return read_val(address, 2, exception);
}
word Disk::read_word(word address, ReadException &exception) {
	return read_val(address, 4, exception);
}

void Disk::write_val(word address, dword val, int n_bytes, WriteException &exception) {
	word p_addr = address >> DISK_PAGE_PSIZE;
	word offset = address & (DISK_PAGE_SIZE - 1);
	CachePage& cpage = get_cpage(p_addr);
	cpage.dirty = true;
	for (int i = 0; i < n_bytes; i++) {
		if (offset == DISK_PAGE_SIZE) {
			offset = 0;
			p_addr++;
			cpage = get_cpage(p_addr);
			cpage.dirty = true;
		}

		cpage.data[offset] = val & 0xFF;
		val >>= 8;
		offset++;
	}
}

void Disk::write_byte(word address, byte data, WriteException &exception) {
	write_val(address, data, 1, exception);
}
void Disk::write_hword(word address, hword data, WriteException &exception) {
	write_val(address, data, 2, exception);
}
void Disk::write_word(word address, word data, WriteException &exception) {
	write_val(address, data, 4, exception);
}


Disk::CachePage& Disk::get_cpage(word p_addr) {
	// CachePage& cpage = m_cache[(p_addr >> DISK_PAGE_PSIZE) & (DISK_CACHE_SIZE - 1)];
	CachePage& cpage = m_cache[(p_addr >> DISK_PAGE_PSIZE) % DISK_CACHE_SIZE];

	cpage.last_acc = n_acc++;
	if (cpage.valid && cpage.p_addr == p_addr) {
		return cpage;
	}

	if (cpage.valid && cpage.dirty) {
		write_cpage(cpage);
	}

	cpage.valid = true;
	cpage.p_addr = p_addr;
	read_cpage(cpage, p_addr);
	return cpage;
}

void Disk::write_cpage(CachePage& cpage) {
	std::ofstream file(m_diskfile.get_path(), std::ios::binary | std::ios::out);
    if (!file.is_open()) {
		lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Error opening disk file");
    }

	file.seekp(cpage.p_addr * DISK_PAGE_SIZE);
	if (!file) {
		file.close();
		lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Error seeking position in disk file");
	}

	std::vector<char> data;
	for (int i = 0; i < DISK_PAGE_SIZE; i++) {
		data.push_back(cpage.data[i]);
	}
	file.write(data.data(), DISK_PAGE_SIZE);
	file.close();
	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Successfully wrote page " << std::to_string(cpage.p_addr) << " to disk");
}

void Disk::read_cpage(CachePage& cpage, word p_addr) {
	std::ifstream file(m_diskfile.get_path(), std::ios::binary | std::ios::in);
    if (!file.is_open()) {
		lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Error opening disk file");
		return;
    }

	file.seekg(cpage.p_addr * DISK_PAGE_SIZE);
	if (!file) {
		file.close();
		lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Error seeking position in disk file");
		return;
	}

	std::vector<char> buffer(DISK_PAGE_SIZE);
	file.read(buffer.data(), DISK_PAGE_SIZE);

	if (!file) {
		file.close();
		lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Error reading from disk file");
		return;
	}

	for (int i = 0; i < DISK_PAGE_SIZE; i++) {
		cpage.data[i] = buffer[i];
	}
	file.close();
	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Successfully read page " << std::to_string(cpage.p_addr) << " from disk");
}

/* When the program ends, we want to save all the pages to disk. Instead of
	creating many I/O streams, just create one and write all pages to disk */
void Disk::write_all() {
	std::ofstream file(m_diskfile.get_path(), std::ios::binary | std::ios::in | std::ios::out);
	if (!file.is_open()) {
		lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Error opening disk file");
		return;
	}

	for (int i = 0; i < DISK_CACHE_SIZE; i++) {
		CachePage& cpage = m_cache[i];
		if (!cpage.dirty || !cpage.valid) {
			continue;
		}

		file.seekp(cpage.p_addr * DISK_PAGE_SIZE);
		if (!file) {
			file.close();
			lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Error seeking position in disk file");
			return;
		}

		std::vector<char> data;
		for (int i = 0; i < DISK_PAGE_SIZE; i++) {
			data.push_back(cpage.data[i]);
		}
		file.write(data.data(), DISK_PAGE_SIZE);

		if (!file) {
			file.close();
			lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Error writing to disk file");
			return;
		}
	}
	file.close();
	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Successfully wrote dirty cache pages to disk");

	/* store disk management info */
	FileWriter fwriter(m_diskfile_manager, std::ios::binary | std::ios::out);
	ByteWriter writer(fwriter);

	FreePage *cur = m_freehead;
	while (cur != nullptr) {
		writer << ByteWriter::Data(cur->p_addr, 4);
		writer << ByteWriter::Data(cur->len, 4);
		cur = cur->next;
	}
	fwriter.close();
}

MockDisk::MockDisk() : Disk(File(".\\shouldnotmakethisfilepls.bin"), static_cast<std::streamsize>(0)) {

}

word MockDisk::get_free_page(PageManagementException& exception) {
	return 0;
}

void MockDisk::return_page(word p_addr, PageManagementException& exception) {

}

void MockDisk::return_all_pages() {

}

void MockDisk::return_pages(word p_addr_lo, word p_addr_hi, PageManagementException& exception) {

}

byte MockDisk::read_byte(word address, ReadException &exception) {
	return 0;
}
hword MockDisk::read_hword(word address, ReadException &exception) {
	return 0;
}
word MockDisk::read_word(word address, ReadException &exception) {
	return 0;
}

void MockDisk::write_byte(word address, byte data, WriteException &exception) {

}
void MockDisk::write_hword(word address, hword data, WriteException &exception) {

}
void MockDisk::write_word(word address, word data, WriteException &exception) {

}

void MockDisk::write_all() {

}