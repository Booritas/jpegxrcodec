#pragma once
#include <cstdint>
#include <cstdio>
#include <vector>

void jpegxr_decompress(FILE* input_file, FILE* outoput_file, uint8_t* buffer, uint32_t buffer_size);
