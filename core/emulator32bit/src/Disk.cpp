#define AEMU_ONLY_CRITICAL_LOG

#include "emulator32bit/disk.h"
#include "util/loggerv2.h"

/*
 * Located at the beginning of disk and the disk page management files
 * to detect invlaid disk/disk management files.
*/
#define MAGIC_HEADER 0x4b534944

#define UNUSED(x) (void)(x)

Disk::Disk(File diskfile, word npages, word lo_page) :
	BaseMemory(npages, lo_page),
	m_free_list(0, npages, false)
{
	this->m_diskfile = diskfile;
	this->m_diskfile_manager = File(diskfile.get_path() + ".info", diskfile.exists());
	this->m_npages = npages;
	this->m_cache = new CachePage[AEMU_DISK_CACHE_SIZE];

	read_disk_files();
}

Disk::Disk() :
	BaseMemory(0, 0),
	m_free_list(0, 0, false)
{
	// maybe this isnt the best way to create support a mocked disk
}

void Disk::read_disk_files()
{
	/*
	 * Read and set up the disk free page manager before reading from the disk memory
	 * in case we have to increase the size of disk memory. Expanding disk size
	 * would mean we would have to add free pages to the disk free page manager FBL,
	 * so it is better to create the disk free page manager before adding such free pages.
	 */
	read_disk_manager_file();

	/* TODO: Determine what it means if m_npages == 0: should we clear the disk files? */
	if (m_npages == 0) {
		return;
	}

	std::ofstream disk_file(m_diskfile.get_path(), std::ios::binary | std::ios::ate | std::ios::out);
    if (!disk_file.is_open()) {
		ERROR("Error opening disk file.");
		return;
    }

	/*
	 * Since output file stream was opened with the 'ios::ate' flag,
	 * the write position 'tellp()' should be located at the end of the file.
	 */
	std::streamsize actual_size = disk_file.tellp();
	const std::streamsize target_size = m_npages * PAGE_SIZE;
	if (actual_size == target_size) {
		disk_file.close();
		return;
	} else if (actual_size > target_size) {
		/*
		 * We don't want to corrupt disk memory by reducing the size
		 * to match the request so stop here.
		 */
		disk_file.close();
		ERROR_SS(std::stringstream() << "Disk file is larger than what is requested. "
				<< std::to_string(actual_size) << " > " << std::to_string(target_size));
		return;
	}

	/*
	 * Disk file size is smaller than what is needed,
	 * we can correct this by increasing the size to what we want.
	 */
	std::streamsize padding_size = target_size - actual_size;
	DEBUG_SS(std::stringstream() << "Padding disk file of size " << std::to_string(actual_size)
			<< " bytes with " << std::to_string(padding_size) << " bytes.");

	std::vector<char> padding(padding_size, 0);
	disk_file.write(padding.data(), padding_size);

	disk_file.close();
	DEBUG_SS(std::stringstream() << "Successfully created disk file of size "
			<< std::to_string(m_npages) << " pages");
}

void Disk::read_disk_manager_file()
{
	FileReader freader(m_diskfile_manager, std::ios::binary | std::ios::in);
	std::vector<byte> bytes;
	while (freader.has_next_byte()) {
		bytes.push_back(freader.read_byte());
	}
	freader.close();
	ByteReader reader(bytes);

	if (!reader.has_next() || reader.read_word() != MAGIC_HEADER) {
		/* set up page managment from scratch since there was no valid header */
		m_free_list.return_block(0, m_npages);

		DEBUG("Creating empty disk.");
		return;
	}

	/* Read in free blocks from the saved file */
	while (reader.has_next()) {
		word page = reader.read_word();
		word len = reader.read_word();

		m_free_list.return_block(page, len);
	}
}

Disk::~Disk()
{
	delete[] this->m_cache;
}

Disk::DiskReadException::DiskReadException(const std::string& msg) :
	message(msg)
{

}

const char* Disk::DiskReadException::what() const noexcept
{
	return message.c_str();
}

Disk::DiskWriteException::DiskWriteException(const std::string& msg) :
	message(msg)
{

}

const char* Disk::DiskWriteException::what() const noexcept
{
	return message.c_str();
}

word Disk::get_free_page()
{
	word addr = m_free_list.get_free_block(1);

	DEBUG_SS(std::stringstream() << "Getting free disk page " << std::to_string(addr));
	return addr;
}

void Disk::return_page(word page)
{
	m_free_list.return_block(page, 1);

	DEBUG_SS(std::stringstream() << "Returning disk page " << std::to_string(page)
			<< " back to disk");
}

void Disk::return_all_pages()
{
	m_free_list.return_all();

	DEBUG("Returning all disk pages back to disk");
}

void Disk::return_pages(word page_lo, word page_hi)
{
	m_free_list.force_return_block(page_lo, page_hi - page_lo + 1);

	DEBUG_SS(std::stringstream() << "Returned all disk pages from " << std::to_string(page_lo)
			<< " to " << std::to_string(page_hi) << " back to disk");
}

std::vector<byte> Disk::read_page(word page)
{
	CachePage& cpage = get_cpage(page);

	std::vector<byte> data;
	for (int i = 0; i < PAGE_SIZE; i++) {
		data.push_back(cpage.data[i]);
	}

	DEBUG_SS(std::stringstream() << "Reading disk page " << std::to_string(page));
	return data;
}

byte Disk::read_byte(word address)
{
	return read_val(address, 1);
}
hword Disk::read_hword(word address)
{
	return read_val(address, 2);
}
word Disk::read_word(word address)
{
	return read_val(address, 4);
}

dword Disk::read_val(word address, int n_bytes)
{
	/* TODO: Add warning for when n_bytes is larger than 8. */

	/* Read from the end since the most significant byte will be located there in little endian. */
	address += n_bytes - 1;
	word page = address >> PAGE_PSIZE;				/* Get the page address (upper bits). */
	word offset = address & (PAGE_SIZE - 1);		/* Offset into the page (lower bits). */
	CachePage& cpage = get_cpage(page);

	dword val = 0;
	for (int i = 0; i < n_bytes; i++) {
		if (offset + 1 == 0) {
			/*
			 * Since we are reading from the end, we might go beyond the beginning of the page.
			 * Correct the offset and page address appropriately when that happens.
			 */
			offset = PAGE_SIZE - 1;
			page--;
			cpage = get_cpage(page);
		}

		val <<= 8;
		val += cpage.data[offset];
		offset--;
	}
	return val;
}

void Disk::write_page(word page, std::vector<byte> data)
{
	if (data.size() != PAGE_SIZE) {
		/* We expect to write a full page to disk. */
		throw DiskWriteException("Tried to write to disk an invalid number of bytes. "
				"Expected " + std::to_string(PAGE_SIZE) + " bytes. Got "
				+ std::to_string(data.size()));
		return;
	}

	CachePage& cpage = get_cpage(page);
	cpage.dirty = true; 							/* Mark as dirty since it is written to. */
	for (int i = 0; i < PAGE_SIZE; i++) {
		cpage.data[i] = data.at(i);
	}

	DEBUG_SS(std::stringstream() << "Wrote to disk page " << std::to_string(cpage.page));
}

void Disk::write_byte(word address, byte data)
{
	write_val(address, data, 1);
}
void Disk::write_hword(word address, hword data)
{
	write_val(address, data, 2);
}
void Disk::write_word(word address, word data)
{
	write_val(address, data, 4);
}

void Disk::write_val(word address, dword val, int n_bytes)
{
	/* TODO: Warn when n_bytes is larger than 8. */

	word page = address >> PAGE_PSIZE;				/* Get the page address (upper bits). */
	word offset = address & (PAGE_SIZE - 1);		/* Offset into the page (lower bits). */
	CachePage& cpage = get_cpage(page);
	cpage.dirty = true;

	/* Write the bytes in little endian. */
	for (int i = 0; i < n_bytes; i++) {
		if (offset == PAGE_SIZE) {
			/*
			 * We might go beyond the end of the page since we are incrementing the address.
			 * Correct the offset and page address appropriately when that happens.
			 */

			offset = 0;
			page++;
			cpage = get_cpage(page);
			cpage.dirty = true;
		}

		cpage.data[offset] = val & 0xFF;			/* Get lower 8 bits. */
		val >>= 8;
		offset++;
	}
}

/* TODO: Perhaps the addr parameter should instead be the page address. It would make more sense. */
Disk::CachePage& Disk::get_cpage(word addr)
{
	if (addr >= m_npages) {
		/* TODO: Should handle case where page is invalid. */
	}

	/* Bitwise AND does the same as modulus to index into table since cache size is a power of 2. */
	CachePage& cpage = m_cache[(addr >> PAGE_PSIZE) & (AEMU_DISK_CACHE_SIZE - 1)];

	cpage.last_acc = n_acc++;						/* LRU information, but unused for now. */
	if (cpage.valid && cpage.page == addr) {
		return cpage;
	}

	if (cpage.valid && cpage.dirty) {
		write_cpage(cpage);
	}

	cpage.valid = true;
	cpage.page = addr;
	read_cpage(cpage);

	DEBUG_SS(std::stringstream() << "Getting cached page " << std::to_string(cpage.page));
	return cpage;
}

void Disk::write_cpage(CachePage& cpage)
{
	/*
	 * Note, even though nothing is being read, std::ios::in has to be passed in otherwise
	 * the file stream will truncate the remaining bytes in the file for some reason.
	 */
	std::ofstream file(m_diskfile.get_path(), std::ios::binary | std::ios::out | std::ios::in);
    if (!file.is_open()) {
		ERROR("Error opening disk file");
    }

	/* Go to location of the cached page in disk so we can write to file. */
	file.seekp(cpage.page << PAGE_PSIZE);
	if (!file) {
		file.close();
		ERROR("Error seeking position in disk file");
	}

	std::vector<char> data;
	for (int i = 0; i < PAGE_SIZE; i++) {
		data.push_back(cpage.data[i]);
	}
	file.write(data.data(), PAGE_SIZE);

	file.close();
	DEBUG_SS(std::stringstream() << "Successfully wrote page " << std::to_string(cpage.page)
			<< " to disk");
}

void Disk::read_cpage(CachePage& cpage)
{
	std::ifstream file(m_diskfile.get_path(), std::ios::binary | std::ios::in);
    if (!file.is_open()) {
		ERROR("Error opening disk file");
		return;
    }

	/* Go to location of the page so we can read from file. */
	file.seekg(cpage.page << PAGE_PSIZE);
	if (!file) {
		file.close();
		ERROR_SS(std::stringstream() << "Error seeking position of page "
				<< std::to_string(cpage.page) << "in disk file");
		return;
	}

	std::vector<char> buffer(PAGE_SIZE);
	file.read(buffer.data(), PAGE_SIZE);

	if (!file) {
		file.close();
		ERROR_SS(std::stringstream() << "Error reading page " << std::to_string(cpage.page)
				<< " from disk file");
		return;
	}

	for (int i = 0; i < PAGE_SIZE; i++) {
		cpage.data[i] = buffer[i];
	}
	file.close();
	DEBUG_SS(std::stringstream() << "Successfully read page " << std::to_string(cpage.page)
			<< " from disk");
}

/*  When the program ends, we want to save all the pages in cache to disk. Instead of
	creating many I/O streams, just create one and write all dirty and valid cache pages to disk. */
void Disk::save()
{
	std::ofstream file(m_diskfile.get_path(), std::ios::binary | std::ios::in | std::ios::out);
	if (!file.is_open()) {
		ERROR("Error opening disk file");
		return;
	}

	/* Write cache pages to file. */
	for (int i = 0; i < AEMU_DISK_CACHE_SIZE; i++) {
		CachePage& cpage = m_cache[i];
		if (!cpage.dirty || !cpage.valid) {
			continue;
		}

		file.seekp(cpage.page << PAGE_PSIZE);
		if (!file) {
			file.close();
			ERROR("Error seeking position in disk file");
			return;
		}

		std::vector<char> data;
		for (int i = 0; i < PAGE_SIZE; i++) {
			data.push_back(cpage.data[i]);
		}
		file.write(data.data(), PAGE_SIZE);

		if (!file) {
			file.close();
			ERROR("Error writing to disk file");
			return;
		}

		DEBUG_SS(std::stringstream() << "WRITING CACHE PAGE TO DISK "
				<< std::to_string(cpage.page));
	}
	file.close();
	DEBUG("Successfully wrote dirty cache pages to disk");

	/* store disk management info. */
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

MockDisk::MockDisk()
{

}

word MockDisk::get_free_page()
{
	return 0;
}

void MockDisk::return_page(word page)
{
	UNUSED(page);
}

void MockDisk::return_all_pages()
{

}

void MockDisk::return_pages(word page_lo, word page_hi)
{
	UNUSED(page_lo);
	UNUSED(page_hi);
}

std::vector<byte> MockDisk::read_page(word page)
{
	UNUSED(page);
	return std::vector<byte>();
}

byte MockDisk::read_byte(word address)
{
	UNUSED(address);
	return 0;
}
hword MockDisk::read_hword(word address)
{
	UNUSED(address);
	return 0;
}
word MockDisk::read_word(word address)
{
	UNUSED(address);
	return 0;
}

void MockDisk::write_page(word page, std::vector<byte> data)
{
	UNUSED(page);
	UNUSED(data);
}

void MockDisk::write_byte(word address, byte data)
{
	UNUSED(address);
	UNUSED(data);
}
void MockDisk::write_hword(word address, hword data)
{
	UNUSED(address);
	UNUSED(data);
}
void MockDisk::write_word(word address, word data)
{
	UNUSED(address);
	UNUSED(data);
}

void MockDisk::save()
{

}