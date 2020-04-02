// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include <cstdlib>
#include "jxrcodec/jxrcodec.hpp"
#include <fstream>


static std::string getTestImagePath(const std::string& imageName)
{
    std::string testImagePath(std::getenv("JPEGXR_TEST_DATA"));
    testImagePath += "/";
    testImagePath += imageName;
    return testImagePath;
}


void testJxrFileDecoder(const std::string& jxrImageName, const std::string& rawImageName)
{

    std::string imagePath = getTestImagePath(jxrImageName);
    std::string rawImagePath = getTestImagePath(rawImageName);

    FILE* fd = fopen(imagePath.c_str(), "rb");
    ASSERT_TRUE(fd!=nullptr);

    uint32_t imageWidth(0), imageHeight(0), imageChannels(3);
    jpegxr_image_info info;
    jpegxr_get_image_info(fd, info);
    std::vector<uint8_t> raster(info.raster_buffer_size);

    jpegxr_decompress(fd, raster.data(), (uint32_t)raster.size());
    fclose(fd);

    std::ifstream trgFile(rawImagePath.c_str(), std::ios::binary);
    std::vector<char> trg((std::istreambuf_iterator<char>(trgFile)), std::istreambuf_iterator<char>());

    ASSERT_EQ(raster.size(), trg.size());
    ASSERT_EQ(memcmp(raster.data(), trg.data(), trg.size()), 0);
}

TEST(jxrcodec, decodeFileWdp)
{
    testJxrFileDecoder("seagull.wdp", "seagull.raw");
}

TEST(jxrcodec, decodeFileJxr)
{
    testJxrFileDecoder("sample.jxr", "sample.raw");
}

void testJxrMemDecoder(const std::string& jxrImageName, const std::string& rawImageName)
{
    std::string imagePath = getTestImagePath(jxrImageName);
    std::string rawImagePath = getTestImagePath(rawImageName);

    std::ifstream srcFile(imagePath.c_str(), std::ios::binary);
    std::vector<unsigned char> src((std::istreambuf_iterator<char>(srcFile)), std::istreambuf_iterator<char>());
    uint32_t imageWidth(0), imageHeight(0), imageChannels(3);

    jpegxr_image_info info;
    jpegxr_get_image_info(src.data(), src.size(), info);
    std::vector<uint8_t> raster(info.raster_buffer_size);

    jpegxr_decompress(src.data(), (uint32_t)src.size(), raster.data(), (uint32_t)raster.size());

    std::ifstream trgFile(rawImagePath.c_str(), std::ios::binary);
    std::vector<char> trg((std::istreambuf_iterator<char>(trgFile)), std::istreambuf_iterator<char>());

    ASSERT_EQ(raster.size(), trg.size());
    ASSERT_EQ(memcmp(raster.data(), trg.data(), trg.size()), 0);
}

void createRaw(const std::string& jxrImageName, const std::string& rawImageName)
{
    std::string imagePath = getTestImagePath(jxrImageName);
    std::string rawImagePath = getTestImagePath(rawImageName);

    std::ifstream srcFile(imagePath.c_str(), std::ios::binary);
    std::vector<unsigned char> src((std::istreambuf_iterator<char>(srcFile)), std::istreambuf_iterator<char>());

    jpegxr_image_info info;
    jpegxr_get_image_info(src.data(), (uint32_t)src.size(), info);
    const int imageSize = info.raster_buffer_size;
    std::vector<uint8_t> raster(imageSize);

    jpegxr_decompress(src.data(), (uint32_t)src.size(), raster.data(), (uint32_t)raster.size());

    std::ofstream fout(rawImagePath.c_str(), std::ios::out | std::ios::binary);
    fout.write((char*)raster.data(), raster.size());
    fout.close();
}

TEST(jxrcodec, decodeBufferWdp)
{
    testJxrMemDecoder("seagull.wdp", "seagull.raw");
}

TEST(jxrcodec, decodeBufferJxr)
{
    testJxrMemDecoder("sample.jxr", "sample.raw");
}

TEST(jxrcodec, decodeBufferJxr2)
{
    testJxrMemDecoder("test.jxr", "test.raw");
}

TEST(jxrcodec, decodeBufferJxr16)
{
    testJxrMemDecoder("tile16.jxr", "tile16.raw");
}

TEST(jxrcodec, decodeBufferJxrTissue)
{
    testJxrMemDecoder("tissue.jxr", "tissue.raw");
}

TEST(jxrcodec, imageInfoMem)
{
    std::string imagePath = getTestImagePath("tissue.jxr");
    std::ifstream srcFile(imagePath.c_str(), std::ios::binary);
    std::vector<unsigned char> src((std::istreambuf_iterator<char>(srcFile)), std::istreambuf_iterator<char>());
    uint32_t width(0), height(0);
    jpegxr_image_info info;
    jpegxr_get_image_info(src.data(), src.size(), info);
    EXPECT_EQ(info.width, 550);
    EXPECT_EQ(info.height, 345);
    EXPECT_EQ(info.channels, 3);
    EXPECT_EQ(info.sample_type, jpegxr_sample_type::Uint);
    EXPECT_EQ(info.sample_size, 1);
    int buffer_size = info.height*info.width*info.channels*info.sample_size;
    EXPECT_EQ(info.raster_buffer_size, buffer_size);
}

TEST(jxrcodec, imageInfoMem16)
{
    std::string imagePath = getTestImagePath("tile16.jxr");
    std::ifstream srcFile(imagePath.c_str(), std::ios::binary);
    std::vector<unsigned char> src((std::istreambuf_iterator<char>(srcFile)), std::istreambuf_iterator<char>());
    uint32_t width(0), height(0);
    jpegxr_image_info info;
    jpegxr_get_image_info(src.data(), (uint32_t)src.size(), info);
    EXPECT_EQ(info.width, 61);
    EXPECT_EQ(info.height, 308);
    EXPECT_EQ(info.channels, 3);
    EXPECT_EQ(info.sample_type, jpegxr_sample_type::Uint);
    EXPECT_EQ(info.sample_size, 2);
    int buffer_size = info.height*info.width*info.channels*info.sample_size;
    EXPECT_EQ(info.raster_buffer_size, buffer_size);
}

TEST(jxrcodec, imageInfoFile)
{
    std::string imagePath = getTestImagePath("tissue.jxr");
    FILE* file = fopen(imagePath.c_str(), "rb");
    uint32_t width(0), height(0);
    jpegxr_image_info info;
    jpegxr_get_image_info(file, info);
    fclose(file);
    EXPECT_EQ(info.width, 550);
    EXPECT_EQ(info.height, 345);
    EXPECT_EQ(info.channels, 3);
    EXPECT_EQ(info.sample_type, jpegxr_sample_type::Uint);
    EXPECT_EQ(info.sample_size, 1);
    int buffer_size = info.height*info.width*info.channels*info.sample_size;
    EXPECT_EQ(info.raster_buffer_size, buffer_size);
}

TEST(jxrcodec, decodeFileJxr2)
{
    testJxrFileDecoder("tile16.jxr", "tile16.raw");
}
