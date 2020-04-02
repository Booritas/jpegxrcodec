#pragma once
#include <cstdint>
#include <cstdio>
#include <vector>

void jpegxr_decompress(FILE* input_file, uint8_t* buffer, uint32_t buffer_size);
void jpegxr_decompress(uint8_t *input_buffer, uint32_t input_buffer_size, uint8_t* buffer, uint32_t buffer_size);
void jpegxr_get_image_size(uint8_t *input_buffer, uint32_t input_buffer_size, uint32_t& width, uint32_t& height);
void jpegxr_get_image_size(FILE* input_file, uint32_t& width, uint32_t& height);
