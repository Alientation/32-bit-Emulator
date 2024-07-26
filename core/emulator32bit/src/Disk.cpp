#include "emulator32bit/Disk.h"

#include "util/Logger.h"

#define MAGIC_HEADER 0x4b534944

Disk::Disk(File diskfile, std::streamsize npages) : m_free_list(0, npages, false) {
	this->m_diskfile = diskfile;
	this->m_diskfile_manager = File(diskfile.get_path() + ".info", diskfile.exists());
	this->m_npages = npages;
	this->m_cache = new CachePage[DISK_CACHE_SIZE];

	if (npages == 0) { // todo, this should clear the files perhaps
		return;
	}

	/* set up disk memory */
	std::ofstream file(m_diskfile.get_path(), std::ios::binary | std::ios::ate | std::ios::out);
    if (!file.is_open()) {
		lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Error opening disk file");
    }

	std::streamsize size = file.tellp();
	if (size == npages * PAGE_SIZE) {
		return;
	} else if (size > npages * PAGE_SIZE) {
		file.close();
		lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Disk file is larger than what is specified");
	}

	std::streamsize padding_size = npages * PAGE_SIZE - size;
	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Padding disk file of size " << std::to_string(size) << " bytes with " << std::to_string(padding_size) << " bytes.");
	std::vector<char> padding(padding_size, 0);
	file.write(padding.data(), padding_size);
	file.close();
	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Successfully created disk file of size " << std::to_string(npages) << " pages");

	/* set up disk free page management */
	FileReader freader(m_diskfile_manager, std::ios::binary | std::ios::in);
	std::vector<byte> bytes;
	while (freader.has_next_byte()) {
		bytes.push_back(freader.read_byte());
	}
	ByteReader reader(bytes);

	/* No valid header */
	if (!reader.has_next() || reader.read_word() != MAGIC_HEADER) {
		/* set up page managment */
		FreeBlockList::Exception exception;
		m_free_list.return_block(0, npages, exception);

		if (exception.type != FreeBlockList::Exception::Type::AOK) {
			lgr::log(lgr::Logger::LogType::ERROR, std::stringstream()
					<< "Failed to create default free block list for disk management for "
					<< std::to_string(npages) << " pages.");
		}

		freader.close();
		return;
	}

	while (reader.has_next()) {
		word p_addr = reader.read_word();
		word len = reader.read_word();

		FreeBlockList::Exception exception;
		m_free_list.return_block(p_addr, len, exception);

		if (exception.type != FreeBlockList::Exception::Type::AOK) {
			lgr::log(lgr::Logger::LogType::ERROR, std::stringstream()
					<< "Failed to create free block list from disk management file.");
		}
	}
	freader.close();
}

Disk::~Disk() {
	delete[] this->m_cache;
}

word Disk::get_free_page(FreeBlockList::Exception& exception) {
	word addr = m_free_list.get_free_block(1, exception);

	if (exception.type != FreeBlockList::Exception::Type::AOK) {
		lgr::log(lgr::Logger::LogType::ERROR, std::stringstream()
				<< "Not enough space to get a free block from disk.");
	}

	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Getting free disk page " << std::to_string(addr));
	return addr;
}

void Disk::return_page(word p_addr, FreeBlockList::Exception& exception) {
	m_free_list.return_block(p_addr, 1, exception);

	if (exception.type != FreeBlockList::Exception::Type::AOK) {
		lgr::log(lgr::Logger::LogType::ERROR, std::stringstream()
				<< "Failed to return page to disk.");
	}

	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Returning disk page "
			<< std::to_string(p_addr) << " back to disk");
}

void Disk::return_all_pages() {
	m_free_list.return_all();

	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Returning all disk pages back to disk");
}

void Disk::return_pages(word p_addr_lo, word p_addr_hi, FreeBlockList::Exception& exception) {
	// for now, this probably isn't too inefficent because of the cached block
	for (; p_addr_lo <= p_addr_hi && exception.type == FreeBlockList::Exception::Type::AOK; p_addr_lo++) {
		return_page(p_addr_lo, exception);

		/* ignore any double free, this just clears all used pages in the range and ignores the rest */
		if (exception.type == FreeBlockList::Exception::Type::DOUBLE_FREE) {
			exception.type = FreeBlockList::Exception::Type::AOK;
		}
	}

	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Returned all disk pages from " << std::to_string(p_addr_lo)
			<< " to " << std::to_string(p_addr_hi) << " back to disk");
}

dword Disk::read_val(word address, int n_bytes, ReadException& exception) {
	address += n_bytes - 1;
	word p_addr = address >> PAGE_PSIZE;
	word offset = address & (PAGE_SIZE - 1);
	dword val = 0;
	CachePage& cpage = get_cpage(p_addr);
	for (int i = 0; i < n_bytes; i++) {
		if (offset == -1) {
			offset = PAGE_SIZE - 1;
			p_addr--;
			cpage = get_cpage(p_addr);
		}

		val <<= 8;
		val += cpage.data[offset];
		offset--;
	}
	return val;
}

std::vector<byte> Disk::read_page(word p_addr, ReadException& exception) {
	CachePage& cpage = get_cpage(p_addr);

	std::vector<byte> data;
	for (int i = 0; i < PAGE_SIZE; i++) {
		data.push_back(cpage.data[i]);
	}

	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Reading disk page "
			<< std::to_string(p_addr));
	return std::move(data);
}

byte Disk::read_byte(word address, ReadException& exception) {
	return read_val(address, 1, exception);
}
hword Disk::read_hword(word address, ReadException& exception) {
	return read_val(address, 2, exception);
}
word Disk::read_word(word address, ReadException& exception) {
	return read_val(address, 4, exception);
}

void Disk::write_val(word address, dword val, int n_bytes, WriteException& exception) {
	word p_addr = address >> PAGE_PSIZE;
	word offset = address & (PAGE_SIZE - 1);
	CachePage& cpage = get_cpage(p_addr);
	cpage.dirty = true;
	for (int i = 0; i < n_bytes; i++) {
		if (offset == PAGE_SIZE) {
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

void Disk::write_page(word p_addr, std::vector<byte> data, WriteException& exception) {
	if (data.size() != PAGE_SIZE) {
		exception.type = WriteException::Type::INVALID_PAGEDATA_SIZE;
		exception.address = p_addr << PAGE_PSIZE;
		exception.data_length = data.size();
		lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Tried to write to disk invalid number of bytes. Expected "
				<< std::to_string(PAGE_SIZE) << ". Got " << std::to_string(data.size()));
		return;
	}

	CachePage& cpage = get_cpage(p_addr);
	cpage.dirty = true;
	for (int i = 0; i < PAGE_SIZE; i++) {
		cpage.data[i] = data.at(i);
	}

	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Writing to disk page "
			<< std::to_string(cpage.p_addr));
}

void Disk::write_byte(word address, byte data, WriteException& exception) {
	write_val(address, data, 1, exception);
}
void Disk::write_hword(word address, hword data, WriteException& exception) {
	write_val(address, data, 2, exception);
}
void Disk::write_word(word address, word data, WriteException& exception) {
	write_val(address, data, 4, exception);
}


Disk::CachePage& Disk::get_cpage(word p_addr) {
	if (p_addr >= m_npages) {
		// todo, should handle case where p_addr is invalid.
	}

	CachePage& cpage = m_cache[(p_addr >> PAGE_PSIZE) & (DISK_CACHE_SIZE - 1)];

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

	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Getting cached page " << std::to_string(cpage.p_addr));
	return cpage;
}

void Disk::write_cpage(CachePage& cpage) {
	std::ofstream file(m_diskfile.get_path(), std::ios::binary | std::ios::out | std::ios::in);
    if (!file.is_open()) {
		lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Error opening disk file");
    }

	file.seekp(cpage.p_addr * PAGE_SIZE);
	if (!file) {
		file.close();
		lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Error seeking position in disk file");
	}

	std::vector<char> data;
	for (int i = 0; i < PAGE_SIZE; i++) {
		data.push_back(cpage.data[i]);
	}
	file.write(data.data(), PAGE_SIZE);
	file.close();
	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Successfully wrote page " << std::to_string(cpage.p_addr) << " to disk");
}

void Disk::read_cpage(CachePage& cpage, word p_addr) {
	std::ifstream file(m_diskfile.get_path(), std::ios::binary | std::ios::in);
    if (!file.is_open()) {
		lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Error opening disk file");
		return;
    }

	file.seekg(cpage.p_addr * PAGE_SIZE);
	if (!file) {
		file.close();
		lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Error seeking position of page " << std::to_string(cpage.p_addr) << "in disk file");
		return;
	}

	std::vector<char> buffer(PAGE_SIZE);
	file.read(buffer.data(), PAGE_SIZE);

	if (!file) {
		file.close();
		lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Error reading page " << std::to_string(cpage.p_addr) << " from disk file");
		return;
	}

	for (int i = 0; i < PAGE_SIZE; i++) {
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

		file.seekp(cpage.p_addr * PAGE_SIZE);
		if (!file) {
			file.close();
			lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Error seeking position in disk file");
			return;
		}

		std::vector<char> data;
		for (int i = 0; i < PAGE_SIZE; i++) {
			data.push_back(cpage.data[i]);
		}
		file.write(data.data(), PAGE_SIZE);

		if (!file) {
			file.close();
			lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Error writing to disk file");
			return;
		}

		lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "WRITING CACHE PAGE TO DISK "
				<< std::to_string(cpage.p_addr));
	}
	file.close();
	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Successfully wrote dirty cache pages to disk");

	/* store disk management info */
	FileWriter fwriter(m_diskfile_manager, std::ios::binary | std::ios::out);
	ByteWriter writer(fwriter);

	std::vector<std::pair<word,word>> blocks = m_free_list.get_blocks();
	writer << ByteWriter::Data(MAGIC_HEADER, 4);
	for (std::pair<word,word> block : blocks) {
		writer << ByteWriter::Data(block.first, 4);
		writer << ByteWriter::Data(block.second, 4);
	}

	fwriter.close();
}

MockDisk::MockDisk() : Disk(File(".\\shouldnotmakethisfilepls.bin"), static_cast<std::streamsize>(0)) {

}

word MockDisk::get_free_page(FreeBlockList::Exception& exception) {
	return 0;
}

void MockDisk::return_page(word p_addr, FreeBlockList::Exception& exception) {

}

void MockDisk::return_all_pages() {

}

void MockDisk::return_pages(word p_addr_lo, word p_addr_hi, FreeBlockList::Exception& exception) {

}

std::vector<byte> MockDisk::read_page(word p_addr, ReadException& exception) {
	return std::vector<byte>();
}

byte MockDisk::read_byte(word address, ReadException& exception) {
	return 0;
}
hword MockDisk::read_hword(word address, ReadException& exception) {
	return 0;
}
word MockDisk::read_word(word address, ReadException& exception) {
	return 0;
}

void MockDisk::write_page(word p_addr, std::vector<byte> data, WriteException& exception) {

}

void MockDisk::write_byte(word address, byte data, WriteException& exception) {

}
void MockDisk::write_hword(word address, hword data, WriteException& exception) {

}
void MockDisk::write_word(word address, word data, WriteException& exception) {

}

void MockDisk::write_all() {

}