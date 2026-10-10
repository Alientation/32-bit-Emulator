
#include "emulator32bit/disk.h"

#include "util/common.h"
#include "util/logger.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <iterator>

// Located at the beginning of disk and the disk page management files
// to detect invlaid disk/disk management files.
#define MAGIC_HEADER 0x4b534944

Disk::Disk(File diskfile, word npages, word lo_page) :
    BaseMemory(npages, lo_page),
    m_cache(kDiskCacheSize),
    m_free_list(0, npages, false)
{
    this->m_diskfile = diskfile;
    this->m_diskfile_manager = File(diskfile.get_path() + ".info", true);
    this->m_npages = npages;

    read_disk_files();
}

Disk::Disk() :
    BaseMemory(0, 0),
    m_free_list(0, 0, false)
{
}

void Disk::read_disk_files()
{
    // Read and set up the disk free page manager before reading from the disk memory
    // in case we have to increase the size of disk memory. Expanding disk size
    // would mean we would have to add free pages to the disk free page manager FBL,
    // so it is better to create the disk free page manager before adding such free pages.
    read_disk_manager_file();

    // TODO: Determine what it means if m_npages == 0: should we clear the disk files?
    if (m_npages == 0)
    {
        return;
    }

    // The file of an existing disk has its contents, it must not be opened in a way that empties
    // it.
    std::error_code error;
    const std::streamsize actual_size =
        std::filesystem::exists(m_diskfile.get_path())
            ? std::streamsize(std::filesystem::file_size(m_diskfile.get_path(), error))
            : 0;
    const std::streamsize target_size = m_npages * kPageSize;
    if (error)
    {
        AEMU_FATAL("Error reading the size of the disk file: {}.", error.message());
        return;
    }
    if (actual_size > target_size)
    {
        // We don't want to corrupt disk memory by reducing the size
        // to match the request so stop here.
        AEMU_FATAL("Disk file is larger than what is requested. {} > {}.", actual_size,
                   target_size);
        return;
    }

    if (actual_size < target_size)
    {
        // Disk file size is smaller than what is needed,
        // we can correct this by increasing the size to what we want.
        std::streamsize padding_size = target_size - actual_size;
        AEMU_DEBUG("Padding disk file of size {} bytes with {} bytes.", actual_size, padding_size);

        std::ofstream disk_file(m_diskfile.get_path(), std::ios::binary | std::ios::app);
        if (!disk_file.is_open())
        {
            AEMU_FATAL("Error opening disk file.");
            return;
        }
        std::vector<char> padding(padding_size, 0);
        disk_file.write(padding.data(), padding_size);
        AEMU_DEBUG("Successfully created disk file of size {} pages.", m_npages);
    }

    // The pages are read and written through this stream, which stays open.
    m_stream.open(m_diskfile.get_path(), std::ios::binary | std::ios::in | std::ios::out);
    if (!m_stream.is_open())
    {
        AEMU_FATAL("Error opening disk file");
    }
}

void Disk::read_disk_manager_file()
{
    FileReader freader(m_diskfile_manager, std::ios::binary | std::ios::in);
    std::vector<byte> bytes;
    while (freader.has_next_byte())
    {
        bytes.push_back(freader.read_byte());
    }
    freader.close();
    ByteReader reader(bytes);

    if (!reader.has_next() || reader.read_word() != MAGIC_HEADER)
    {
        // set up page managment from scratch since there was no valid header
        m_free_list.return_block(0, m_npages);

        AEMU_DEBUG("Creating empty disk.");
        return;
    }

    // Read in free blocks from the saved file
    while (reader.has_next())
    {
        word page = reader.read_word();
        word len = reader.read_word();

        m_free_list.return_block(page, len);
    }
}

Disk::~Disk() = default;

Disk::DiskReadException::DiskReadException(const std::string &msg) :
    message(msg)
{
}

const char *Disk::DiskReadException::what() const noexcept
{
    return message.c_str();
}

Disk::DiskWriteException::DiskWriteException(const std::string &msg) :
    message(msg)
{
}

const char *Disk::DiskWriteException::what() const noexcept
{
    return message.c_str();
}

word Disk::get_free_page()
{
    word addr = m_free_list.get_free_block(1);

    // A page that was used before still has its old contents. Whoever gets the page starts with
    //       zeros, not with the data of a page that was freed.
    CachePage &cpage = get_cpage(addr, false);
    std::fill(std::begin(cpage.data), std::end(cpage.data), byte(0));
    cpage.dirty = true;

    AEMU_DEBUG("Getting free disk page {}.", addr);
    return addr;
}

void Disk::return_page(word page)
{
    m_free_list.return_block(page, 1);

    AEMU_DEBUG("Returning disk page {} back to disk.", page);
}

void Disk::return_all_pages()
{
    m_free_list.return_all();

    AEMU_DEBUG("Returning all disk pages back to disk");
}

void Disk::return_pages(word page_lo, word page_hi)
{
    m_free_list.force_return_block(page_lo, page_hi - page_lo + 1);

    AEMU_DEBUG("Returned all disk pages from {} to {} back to disk.", page_lo, page_hi);
}

std::vector<byte> Disk::read_page(word page)
{
    std::vector<byte> data(kPageSize);
    read_page(page, data.data());
    return data;
}

void Disk::read_page(word page, byte *out)
{
    const CachePage &cpage = get_cpage(page);
    std::memcpy(out, cpage.data, kPageSize);

    AEMU_DEBUG("Reading disk page {}.", page);
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
    // TODO: Add warning for when n_bytes is larger than 8.

    // Addresses are absolute, disk pages are relative to the start of the disk.
    address -= m_start_addr;

    // Read from the end since the most significant byte will be located there in little endian.
    address += n_bytes - 1;
    word page = address >> kNumPageOffsetBits; // Get the page address (upper bits).
    word offset = address & (kPageSize - 1); // Offset into the page (lower bits).
    CachePage *cpage = &get_cpage(page);

    dword val = 0;
    for (int i = 0; i < n_bytes; i++)
    {
        if (offset + 1 == 0)
        {
            // Since we are reading from the end, we might go beyond the beginning of the page.
            // Correct the offset and page address appropriately when that happens.
            offset = kPageSize - 1;
            page--;
            cpage = &get_cpage(page);
        }

        val <<= 8;
        val += cpage->data[offset];
        offset--;
    }
    return val;
}

void Disk::write_page(word page, const std::vector<byte> &data)
{
    if (data.size() != kPageSize)
    {
        // We expect to write a full page to disk.
        throw DiskWriteException("Tried to write to disk an invalid number of bytes. "
                                 "Expected "
                                 + std::to_string(kPageSize) + " bytes. Got "
                                 + std::to_string(data.size()));
        return;
    }

    write_page(page, data.data());
}

void Disk::write_page(word page, const byte *data)
{
    // The whole page is replaced, so what the file has of it is not needed.
    CachePage &cpage = get_cpage(page, false);
    cpage.dirty = true; // Mark as dirty since it is written to.
    std::memcpy(cpage.data, data, kPageSize);

    AEMU_DEBUG("Wrote to disk page {}.", cpage.page);
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
    // TODO: Warn when n_bytes is larger than 8.

    // Addresses are absolute, disk pages are relative to the start of the disk.
    address -= m_start_addr;

    word page = address >> kNumPageOffsetBits; // Get the page address (upper bits).
    word offset = address & (kPageSize - 1); // Offset into the page (lower bits).
    CachePage *cpage = &get_cpage(page);
    cpage->dirty = true;

    // Write the bytes in little endian.
    for (int i = 0; i < n_bytes; i++)
    {
        if (offset == kPageSize)
        {
            // We might go beyond the end of the page since we are incrementing the address.
            // Correct the offset and page address appropriately when that happens.

            offset = 0;
            page++;
            cpage = &get_cpage(page);
            cpage->dirty = true;
        }

        cpage->data[offset] = val & 0xFF; // Get lower 8 bits.
        val >>= 8;
        offset++;
    }
}

Disk::CachePage &Disk::get_cpage(word addr, bool load)
{
    if (addr >= m_npages)
    {
        throw DiskReadException("Disk page " + std::to_string(addr)
                                + " is out of range, the disk has " + std::to_string(m_npages)
                                + " pages.");
    }

    // Bitwise AND does the same as modulus to index into table since cache size is a power of 2.
    CachePage &cpage = m_cache[addr & (kDiskCacheSize - 1)];

    if (cpage.valid && cpage.page == addr)
    {
        return cpage;
    }

    if (cpage.valid && cpage.dirty)
    {
        write_cpage(cpage);
    }

    // The cache page holds the new page, which matches the file until it is written to.
    cpage.valid = true;
    cpage.page = addr;
    cpage.dirty = false;
    if (load)
    {
        read_cpage(cpage);
    }

    AEMU_DEBUG("Getting cached page {}.", cpage.page);
    return cpage;
}

void Disk::write_cpage(CachePage &cpage)
{
    // Go to location of the cached page in disk so we can write to file.
    m_stream.clear();
    m_stream.seekp(std::streamoff(U64(cpage.page) << kNumPageOffsetBits));
    m_stream.write(reinterpret_cast<const char *>(cpage.data), kPageSize);
    m_stream.flush();
    AEMU_CHECK(bool(m_stream), "Error writing page {} to the disk file.",
               cpage.page);

    cpage.dirty = false;
    AEMU_DEBUG("Successfully wrote page {} to disk.", cpage.page);
}

void Disk::read_cpage(CachePage &cpage)
{
    // Go to location of the page so we can read from file.
    m_stream.clear();
    m_stream.seekg(std::streamoff(U64(cpage.page) << kNumPageOffsetBits));
    m_stream.read(reinterpret_cast<char *>(cpage.data), kPageSize);
    AEMU_CHECK(bool(m_stream), "Error reading page {} from the disk file.",
               cpage.page);

    AEMU_DEBUG("Successfully read page {} from disk.", cpage.page);
}

void Disk::save()
{
    if (m_npages == 0)
    {
        return;
    }

    // Write cache pages to file.
    for (CachePage &cpage : m_cache)
    {
        if (cpage.dirty && cpage.valid)
        {
            write_cpage(cpage);
        }
    }
    m_stream.flush();
    AEMU_CHECK(bool(m_stream), "Error writing to the disk file.");
    AEMU_DEBUG("Successfully wrote dirty cache pages to disk");

    // store disk management info.
    FileWriter fwriter(m_diskfile_manager, std::ios::binary | std::ios::out);
    ByteWriter writer(fwriter);

    std::vector<std::pair<word, word>> blocks = m_free_list.get_blocks();
    writer << ByteWriter::Data(MAGIC_HEADER, 4);
    for (std::pair<word, word> block : blocks)
    {
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
    // Pages that were returned are cleared, a page starts as zeros whatever it was used for.
    if (!m_returned.empty())
    {
        const word page = m_returned.back();
        m_returned.pop_back();
        m_is_returned[page] = false;
        return page;
    }
    m_is_returned.push_back(false);
    return m_next_page++;
}

void MockDisk::return_page(word page)
{
    if (page < m_next_page && !m_is_returned[page])
    {
        if (page < m_pages.size() && !m_pages[page].empty())
        {
            m_spare.push_back(std::move(m_pages[page]));
            m_pages[page].clear();
        }
        m_is_returned[page] = true;
        m_returned.push_back(page);
    }
}

void MockDisk::return_all_pages()
{
    m_pages.clear();
    m_spare.clear();
    m_returned.clear();
    m_is_returned.clear();
    m_next_page = 0;
}

void MockDisk::return_pages(word page_lo, word page_hi)
{
    for (word page = page_lo; page <= page_hi && page < m_next_page; page++)
    {
        return_page(page);
    }
}

void MockDisk::read_page(word page, byte *out)
{
    // Virtual memory fetches a full page from disk the first time a page is touched, so hand back
    // a zeroed page rather than an empty one.
    if (page < m_pages.size() && !m_pages[page].empty())
    {
        std::memcpy(out, m_pages[page].data(), kPageSize);
    }
    else
    {
        std::memset(out, 0, kPageSize);
    }
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

void MockDisk::write_page(word page, const byte *data)
{
    if (page >= m_pages.size())
    {
        m_pages.resize(std::size_t(page) + 1);
    }
    std::vector<byte> &contents = m_pages[page];
    if (contents.empty())
    {
        if (!m_spare.empty())
        {
            contents = std::move(m_spare.back());
            m_spare.pop_back();
        }
        else
        {
            contents.resize(kPageSize);
        }
    }
    std::memcpy(contents.data(), data, kPageSize);
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