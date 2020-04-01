# Jpeg XR codec (v1.0)
## Overview
The source of the software based on version 1.41 of [JPEG XR Reference Codec](https://jpeg.org/jpegxr/software.html). The only changes to the original software related to reading of input data and writing of output data. It shall allow reading/writing from/to memory blocks as well as from/to files.
## API Reference
```c++
void jpegxr_decompress(FILE* input_file, uint8_t* buffer, uint32_t buffer_size);
void jpegxr_decompress(uint8_t *input_buffer, uint32_t input_buffer_size, uint8_t* buffer, uint32_t buffer_size);
```
The first function reads jxr stream from file "input_file", decode it and saves decoded raster in the output buffer.
Second function reads jxr stream from memory buffer. 
## Building
* Change current directory to the software root folder.
```bash
mkdir build
cd build
cmake ..
cmake --build .
```
