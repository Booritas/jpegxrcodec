#pragma once
#include <cstdint>
#include <cstdio>
#include <vector>

void jpegxr_decompress(FILE* fd, std::vector<uint8_t>& raster);
int jpegxr_decompress(const char*path_in, std::vector<uint8_t>& raster);
