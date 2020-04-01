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
    jpegxr_get_image_size(fd, imageWidth, imageHeight);
    const int imageSize = imageWidth*imageHeight*imageChannels;
    std::vector<uint8_t> raster(imageSize);

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

    jpegxr_get_image_size(src.data(), src.size(), imageWidth, imageHeight);

    const int imageSize = imageWidth*imageHeight*imageChannels;
    std::vector<uint8_t> raster(imageSize);

    jpegxr_decompress(src.data(), (uint32_t)src.size(), raster.data(), (uint32_t)raster.size());

    std::ifstream trgFile(rawImagePath.c_str(), std::ios::binary);
    std::vector<char> trg((std::istreambuf_iterator<char>(trgFile)), std::istreambuf_iterator<char>());

    ASSERT_EQ(raster.size(), trg.size());
    ASSERT_EQ(memcmp(raster.data(), trg.data(), trg.size()), 0);
}

void createRaw(const std::string& jxrImageName, const std::string& rawImageName, int imageWidth, int imageHeight, int imageChannels)
{
    const int imageSize = imageWidth*imageHeight*imageChannels;
    std::vector<uint8_t> raster(imageSize);
    std::string imagePath = getTestImagePath(jxrImageName);
    std::string rawImagePath = getTestImagePath(rawImageName);

    std::ifstream srcFile(imagePath.c_str(), std::ios::binary);
    std::vector<unsigned char> src((std::istreambuf_iterator<char>(srcFile)), std::istreambuf_iterator<char>());

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

TEST(jxrcodec, decodeBufferJxrTissue)
{
    testJxrMemDecoder("tissue.jxr", "tissue.raw");
}

TEST(jxrcodec, imageSizeMem)
{
    std::string imagePath = getTestImagePath("tissue.jxr");
    std::ifstream srcFile(imagePath.c_str(), std::ios::binary);
    std::vector<unsigned char> src((std::istreambuf_iterator<char>(srcFile)), std::istreambuf_iterator<char>());
    uint32_t width(0), height(0);
    jpegxr_get_image_size(src.data(), (uint32_t)src.size(), width, height);
    EXPECT_EQ(width, 550);
    EXPECT_EQ(height, 345);
}

TEST(jxrcodec, imageSizeFile)
{
    std::string imagePath = getTestImagePath("tissue.jxr");
    FILE* file = fopen(imagePath.c_str(), "rb");
    uint32_t width(0), height(0);
    jpegxr_get_image_size(file, width, height);
    fclose(file);
    EXPECT_EQ(width, 550);
    EXPECT_EQ(height, 345);
}
