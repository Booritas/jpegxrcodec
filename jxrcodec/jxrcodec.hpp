#pragma once
#include <cstdint>
#include <cstdio>
#include <vector>

void jpegxr_decompress(FILE* fd, const char* path_out, uint8_t* buffer, uint32_t buffer_size);