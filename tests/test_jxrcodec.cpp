// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include <cstdlib>
#include "jxrcodec/jxrcodec.hpp"


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
    std::vector<uint8_t> raster;
    std::string imagePath = getTestImagePath("seagull.wdp");
    jpegxr_decompress(imagePath.c_str(), raster);
    //std::shared_ptr<JxrFile> file = JxrFile::fromFile(imagePath);
}
