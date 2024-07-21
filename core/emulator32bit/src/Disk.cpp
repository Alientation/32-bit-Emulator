#include "emulator32bit/Disk.h"

#include "util/Logger.h"

Disk::Disk(File diskfile, std::streamsize npages) {
	this->m_diskfile = diskfile;
	this->m_npages = npages;
	this->m_cache = new CachePage[DISK_CACHE_SIZE];

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
}

Disk::~Disk() {
	delete[] this->m_cache;
}

dword Disk::read_val(word address, int n_bytes, ReadException &exception) {
	word p_addr = bitfield_u32(address + n_bytes - 1, 12, 20);
	word offset = bitfield_u32(address + n_bytes - 1, 0, 12);
	dword val = 0;
	CachePage& cpage = get_cpage(p_addr);
	for (int i = n_bytes-1; i >= 0; i--) {
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
	word p_addr = bitfield_u32(address, 12, 20);
	word offset = bitfield_u32(address, 0, 12);
	CachePage& cpage = get_cpage(p_addr);
	for (int i = n_bytes-1; i >= 0; i--) {
		if (offset == DISK_PAGE_SIZE) {
			offset = 0;
			p_addr++;
			cpage = get_cpage(p_addr);
		}

		cpage.data[offset] = val & 0xFF;
		val >>= 8;
		offset++;
	}
}

void Disk::write_byte(word address, byte data, WriteException &exception) {
	write_val(address, 1, data, exception);
}
void Disk::write_hword(word address, hword data, WriteException &exception) {
	write_val(address, 2, data, exception);
}
void Disk::write_word(word address, word data, WriteException &exception) {
	write_val(address, 4, data, exception);
}


Disk::CachePage& Disk::get_cpage(word p_addr) {
	CachePage& cpage = m_cache[(p_addr >> DISK_PAGE_PSIZE) & (DISK_CACHE_SIZE - 1)];

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
    }

	file.seekg(cpage.p_addr * DISK_PAGE_SIZE);
	if (!file) {
		file.close();
		lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Error seeking position in disk file");
	}

	std::vector<char> buffer(DISK_PAGE_SIZE);
	file.read(buffer.data(), DISK_PAGE_SIZE);
	for (int i = 0; i < DISK_PAGE_SIZE; i++) {
		cpage.data[i] = buffer[i];
	}
	file.close();
	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Successfully read page " << std::to_string(cpage.p_addr) << " from disk");
}

/* When the program ends, we want to save all the pages to disk. Instead of
	creating many I/O streams, just create one and write all pages to disk */
void Disk::write_all() {
	std::ofstream file(m_diskfile.get_path(), std::ios::binary | std::ios::out);
	if (!file.is_open()) {
		lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Error opening disk file");
    }

	for (int i = 0; i < DISK_CACHE_SIZE; i++) {
		CachePage& cpage = m_cache[i];
		if (!m_cache->dirty || !m_cache->valid) {
			continue;
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
	}
	file.close();
	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Successfully wrote dirty cache pages to disk");
}