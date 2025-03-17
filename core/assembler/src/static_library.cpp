#include "assembler/static_library.h"

#include <fstream>


void WriteStaticLibrary(std::vector<File>& objs, File out)
{
    // clearing library file
    std::ofstream ofs;
    ofs.open(out.get_path(), std::ofstream::out | std::ofstream::trunc);
    ofs.close();

    FileWriter writer = FileWriter(out, std::ios::out | std::ios::binary);
    ByteWriter b_writer(writer);

    b_writer << ByteWriter::Data(objs.size(), 8);
    std::vector<std::vector<byte>> bytes;
    for (File file : objs)
    {
        FileReader reader(file, std::ios::in | std::ios::binary);
        bytes.push_back(std::vector<byte>());
        while (reader.has_next_byte())
        {
            bytes.back().push_back(reader.read_byte());
        }
        reader.close();
    }

    for (std::vector<byte>& data : bytes)
    {
        b_writer << ByteWriter::Data(data.size(), 8);
        for (byte b : data)
        {
            b_writer << ByteWriter::Data(b, 1);
        }
    }

    writer.close();
}

void ReadStaticLibrary(std::vector<ObjectFile>& objs, File in)
{
    FileReader reader = FileReader(in, std::ios::in | std::ios::binary);

    std::vector<byte> bytes;
    while (reader.has_next_byte())
    {
        bytes.push_back(reader.read_byte());
    }
    ByteReader b_reader(bytes);

    unsigned long long n_objs = b_reader.read_dword();
    for (unsigned long long i = 0; i < n_objs; i++)
    {
        unsigned long long size = b_reader.read_dword();

        std::vector<byte> data;
        for (unsigned long long b_i = 0; b_i < size; b_i++)
        {
            data.push_back(b_reader.read_byte());
        }
        ObjectFile obj;
        obj.read_object_file(data);
        objs.push_back(obj);
    }

    reader.close();
}
