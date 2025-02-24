﻿// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include <cstdlib>
#include "jxrcodec/jxrcodec.hpp"
#include <fstream>
#include "jxrcodec/codec/bytestream.h"


static void bs_fill_noise(byte_stream& bs)
{
    bs.buffer_pos = (uint8_t*)0xffff;
    bs.buffer_begin = (uint8_t*)0xFFFF;
    bs.buffer_end = (uint8_t*)0xFFFF;
    bs.file = (FILE*)0xFFFF;
    bs.file2 = (FILE*)0xFFFF;
}

TEST(bitstream, bytestream_copy)
{
    byte_stream bs_src, bs_trg;
    {
        bs_fill_noise(bs_src);
        EXPECT_NE(memcmp(&bs_src, &bs_trg, sizeof(byte_stream)), 0);
        bs_copy(&bs_trg, &bs_src);
        EXPECT_EQ(memcmp(&bs_src, &bs_trg, sizeof(byte_stream)), 0);
    }
}
TEST(bitstream, bytestream_init_mem)
{
    byte_stream bs;
    {
        bs_fill_noise(bs);
        std::vector<uint8_t> buffer(100);
        bs_init_mem(&bs, buffer.data(), buffer.size(), 0);
        EXPECT_EQ(bs_is_ready(&bs), 0);
        EXPECT_EQ(bs.buffer_pos, nullptr);
        EXPECT_EQ(bs.file, nullptr);
        EXPECT_EQ(bs.file2, nullptr);
        EXPECT_EQ(bs.buffer_begin, buffer.data());
        EXPECT_EQ(bs.buffer_end, buffer.data()+buffer.size());
        bs_make_ready(&bs);
        EXPECT_EQ(bs_is_ready(&bs), 1);
        EXPECT_EQ(bs.buffer_pos, buffer.data());
        EXPECT_EQ(bs_is_memory_stream(&bs), 1);
    }
    {
        bs_fill_noise(bs);
        std::vector<uint8_t> buffer(100);
        bs_init_mem(&bs, buffer.data(), buffer.size(), 1);
        EXPECT_EQ(bs_is_ready(&bs), 1);
        EXPECT_EQ(bs.buffer_pos, buffer.data());
        EXPECT_EQ(bs.file, nullptr);
        EXPECT_EQ(bs.file2, nullptr);
        EXPECT_EQ(bs.buffer_begin, buffer.data());
        EXPECT_EQ(bs.buffer_end, buffer.data()+buffer.size());
        EXPECT_EQ(bs_is_memory_stream(&bs), 1);
    }
}

TEST(bitstream, bytestream_init_file)
{
    byte_stream bs;
    {
        bs_fill_noise(bs);
        FILE* ptr = (FILE*)0xFFFFCCCCFFFFCCCC;
        bs_init_file(&bs, ptr, 0);
        EXPECT_EQ(bs_is_ready(&bs), 0);
        EXPECT_EQ(bs.buffer_pos, nullptr);
        EXPECT_EQ(bs.file, nullptr);
        EXPECT_EQ(bs.file2, ptr);
        EXPECT_EQ(bs.buffer_begin, nullptr);
        EXPECT_EQ(bs.buffer_end, nullptr);
        bs_make_ready(&bs);
        EXPECT_EQ(bs_is_ready(&bs), 1);
        EXPECT_EQ(bs.buffer_pos, nullptr);
        EXPECT_EQ(bs.file, ptr);
        EXPECT_EQ(bs_is_memory_stream(&bs), 0);
    }
    {
        bs_fill_noise(bs);
        FILE* ptr = (FILE*)0xFFFFCCCCFFFFCCCC;
        bs_init_file(&bs, ptr, 1);
        EXPECT_EQ(bs_is_ready(&bs), 1);
        EXPECT_EQ(bs.buffer_pos, nullptr);
        EXPECT_EQ(bs.file, ptr);
        EXPECT_EQ(bs.file2, ptr);
        EXPECT_EQ(bs.buffer_begin, nullptr);
        EXPECT_EQ(bs.buffer_end, nullptr);
    }
}

TEST(bitstream, bytestream_seek_mem)
{
    byte_stream bs;
    std::vector<uint8_t> buffer(100);
    bs_init_mem(&bs, buffer.data(), buffer.size(), 1);
    int ret = bs_seek(&bs, 50, SEEK_SET);
    EXPECT_EQ(ret, 1);
    int pos = bs_tell(&bs);
    EXPECT_EQ(pos, 50);
    ret = bs_seek(&bs, 20, SEEK_CUR);
    EXPECT_EQ(ret, 1);
    pos = bs_tell(&bs);
    EXPECT_EQ(pos, 70);
    ret = bs_seek(&bs, 1000, SEEK_CUR);
    EXPECT_EQ(ret, 0);
    pos = bs_tell(&bs);
    EXPECT_EQ(pos, 70);
    ret = bs_seek(&bs, -10, SEEK_END);
    EXPECT_EQ(ret, 1);
    pos = bs_tell(&bs);
    EXPECT_EQ(pos, 90);
}

TEST(bitstream, bytestream_seek_file)
{
    std::vector<uint8_t> buffer(100);
    std::string imagePath(std::tmpnam(nullptr));
    for(size_t index = 0; index<buffer.size(); ++index)
    {
        buffer[index] = (uint8_t)index;
    }
    {
        std::ofstream fout(imagePath.c_str(), std::ios::out | std::ios::binary);
        fout.write((char*)buffer.data(), buffer.size());
        fout.close();
    }

    byte_stream bs;
    FILE* file = fopen(imagePath.c_str(),"rb");
    bs_init_file(&bs, file, 1);
    int ret = bs_seek(&bs, 50, SEEK_SET);
    EXPECT_EQ(ret, 1);
    int pos = bs_tell(&bs);
    EXPECT_EQ(pos, 50);
    ret = bs_seek(&bs, 20, SEEK_CUR);
    EXPECT_EQ(ret, 1);
    pos = bs_tell(&bs);
    EXPECT_EQ(pos, 70);
    //ret = bs_seek(&bs, 1000, SEEK_CUR);
    //EXPECT_EQ(ret, 0);
    //pos = bs_tell(&bs);
    //EXPECT_EQ(pos, 70);
    ret = bs_seek(&bs, -10, SEEK_END);
    EXPECT_EQ(ret, 1);
    pos = bs_tell(&bs);
    EXPECT_EQ(pos, 90);
    fclose(file);
    remove(imagePath.c_str());
}

TEST(bitstream, bytestream_read_file)
{
    byte_stream bs;
    std::vector<uint8_t> buffer(100);
    for(size_t index = 0; index<buffer.size(); ++index)
    {
        buffer[index] = (uint8_t)index;
    }
    std::string imagePath(std::tmpnam(nullptr));
    {
        std::ofstream fout(imagePath.c_str(), std::ios::out | std::ios::binary);
        fout.write((char*)buffer.data(), buffer.size());
        fout.close();
    }

    FILE* file = fopen(imagePath.c_str(),"rb");
    bs_init_file(&bs, file, 1);
    std::vector<uint8_t> buf2(10), buf3(10);
    for(size_t index = 0; index<buf2.size(); ++index)
    {
        buf2[index] = (uint8_t)index + 10;
    }
    bs_seek(&bs, 10, SEEK_SET);
    bs_read(&bs, buf3.data(), buf3.size());
    EXPECT_EQ(memcmp(buf3.data(), buf2.data(), buf2.size()), 0);
    for(size_t index = 0; index<buf2.size(); ++index)
    {
        buf2[index] = (uint8_t)index + 20;
    }
    bs_read(&bs, buf3.data(), buf3.size());
    EXPECT_EQ(memcmp(buf3.data(), buf2.data(), buf2.size()), 0);
    fclose(file);
    remove(imagePath.c_str());
}

TEST(bitstream, bytestream_read_mem)
{
    byte_stream bs;
    std::vector<uint8_t> buffer(100);
    for(size_t index = 0; index<buffer.size(); ++index)
    {
        buffer[index] = (uint8_t)index;
    }
    bs_init_mem(&bs, buffer.data(), buffer.size(), 1);
    std::vector<uint8_t> buf2(10), buf3(10);
    for(size_t index = 0; index<buf2.size(); ++index)
    {
        buf2[index] = (uint8_t)index + 10;
    }
    bs_seek(&bs, 10, SEEK_SET);
    bs_read(&bs, buf3.data(), buf3.size());
    EXPECT_EQ(memcmp(buf3.data(), buf2.data(), buf2.size()), 0);
    for(size_t index = 0; index<buf2.size(); ++index)
    {
        buf2[index] = (uint8_t)index + 20;
    }
    bs_read(&bs, buf3.data(), buf3.size());
    EXPECT_EQ(memcmp(buf3.data(), buf2.data(), buf2.size()), 0);
}

TEST(bitstream, bytestream_write_mem)
{
    byte_stream bs;
    std::vector<uint8_t> buffer(100);
    for(size_t index = 0; index<buffer.size(); ++index)
    {
        buffer[index] = (uint8_t)index;
    }
    bs_init_mem(&bs, buffer.data(), buffer.size(), 1);
    for(int patch=0; patch<100; patch+=10)
    {
        int written = bs_write(&bs, buffer.data()+patch, 10);
        EXPECT_EQ(written, 10);
    }
    std::vector<uint8_t> buf2(10), buf3(10);
    for(size_t index = 0; index<buf2.size(); ++index)
    {
        buf2[index] = (uint8_t)index + 10;
    }
    bs_seek(&bs, 10, SEEK_SET);
    bs_read(&bs, buf3.data(), buf3.size());
    EXPECT_EQ(memcmp(buf3.data(), buf2.data(), buf2.size()), 0);
    for(size_t index = 0; index<buf2.size(); ++index)
    {
        buf2[index] = (uint8_t)index + 20;
    }
    bs_read(&bs, buf3.data(), buf3.size());
    EXPECT_EQ(memcmp(buf3.data(), buf2.data(), buf2.size()), 0);
}

TEST(bitstream, bytestream_write_file)
{
    byte_stream bs;
    std::vector<uint8_t> buffer(100);
    for(size_t index = 0; index<buffer.size(); ++index)
    {
        buffer[index] = (uint8_t)index;
    }
    std::string imagePath(std::tmpnam(nullptr));
    FILE* file = fopen(imagePath.c_str(), "w+b");
    bs_init_file(&bs, file, 1);
    for(int patch=0; patch<100; patch+=10)
    {
        int written = bs_write(&bs, buffer.data()+patch, 10);
        EXPECT_EQ(written, 10);
    }
    std::vector<uint8_t> buf2(10), buf3(10);
    for(size_t index = 0; index<buf2.size(); ++index)
    {
        buf2[index] = (uint8_t)index + 10;
    }
    bs_seek(&bs, 10, SEEK_SET);
    bs_read(&bs, buf3.data(), buf3.size());
    EXPECT_EQ(memcmp(buf3.data(), buf2.data(), buf2.size()), 0);
    for(size_t index = 0; index<buf2.size(); ++index)
    {
        buf2[index] = (uint8_t)index + 20;
    }
    bs_read(&bs, buf3.data(), buf3.size());
    EXPECT_EQ(memcmp(buf3.data(), buf2.data(), buf2.size()), 0);
    fclose(file);
}
