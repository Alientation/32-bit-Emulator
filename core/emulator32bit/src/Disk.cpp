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
}

Disk::~Disk() {
	delete[] this->m_cache;
}

word Disk::get_free_page() {
	return 0;
}

void Disk::return_page(word p_addr) {

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
}

MockDisk::MockDisk() : Disk(File(".\\shouldnotmakethisfilepls.bin"), static_cast<std::streamsize>(0)) {

}

word MockDisk::get_free_page() {
	return 0;
}

void MockDisk::return_page(word p_addr) {

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