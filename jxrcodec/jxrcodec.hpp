#pragma once
#include <cstdint>
#include <cstdio>
#include <vector>

struct jpegxr_image_info
{
    uint32_t width;
    uint32_t height;
    uint32_t channels;
    uint32_t sample_size;
    uint32_t raster_buffer_size;
    bool bit_mask;
};

void jpegxr_decompress(FILE* input_file, uint8_t* buffer, uint32_t buffer_size);
void jpegxr_decompress(uint8_t *input_buffer, uint32_t input_buffer_size, uint8_t* buffer, uint32_t buffer_size);
void jpegxr_get_image_info(uint8_t *input_buffer, uint32_t input_buffer_size, jpegxr_image_info& info);
void jpegxr_get_image_info(FILE* input_file, jpegxr_image_info& info);
