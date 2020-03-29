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


TEST(jxrcodec, openMissingFile)
{
    //std::string imagePath = getTestImagePath("aaaaa");
    //EXPECT_THROW(JxrFile::fromFile(imagePath), std::runtime_error);
}

TEST(jxrcodec, openFile)
{
    const int image_width = 1000;
    const int image_height = 667;
    const int image_channels = 3;
    const int image_size = image_width*image_height*image_channels;

    std::vector<uint8_t> raster;
    std::string imagePath = getTestImagePath("seagull.wdp");
    std::string decodedImagePath = getTestImagePath("seagull.raw");
    std::string outputImagePath(std::tmpnam(nullptr));
    outputImagePath += ".raw";

    FILE* fd = fopen(imagePath.c_str(), "rb");
    FILE* output_file = fopen(outputImagePath.c_str(), "w+b");
    ASSERT_TRUE(fd!=nullptr);
    jpegxr_decompress(fd, output_file, raster.data(), raster.size());
    fclose(fd);
    fclose(output_file);

    {
        std::ifstream srcFile(outputImagePath.c_str(), std::ios::binary);
        std::vector<char> src((std::istreambuf_iterator<char>(srcFile)), std::istreambuf_iterator<char>());

        std::ifstream trgFile(decodedImagePath.c_str(), std::ios::binary);
        std::vector<char> trg((std::istreambuf_iterator<char>(trgFile)), std::istreambuf_iterator<char>());

        ASSERT_EQ(src.size(), trg.size());
        ASSERT_EQ(memcmp(src.data(), trg.data(), src.size()), 0);
    }
    remove(outputImagePath.c_str());
}
