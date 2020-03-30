#pragma once
#include <stdio.h>

# ifdef __cplusplus
# define JXR_EXTERN extern "C"
# else
# define JXR_EXTERN extern
# endif

struct byte_stream
{
    unsigned char* buffer_begin;
    unsigned char* buffer_end;
    unsigned char* buffer_pos;
    unsigned char* buffer_pos2;
    FILE* file;
    FILE* file2; // storage for the file before it is ready
};

JXR_EXTERN void bs_init_mem(struct byte_stream* str, unsigned char* buffer, size_t size, int make_ready);
JXR_EXTERN void bs_make_ready(struct byte_stream* bs);
JXR_EXTERN void bs_make_unready(struct byte_stream* bs);
JXR_EXTERN void bs_init_file(struct byte_stream* str, FILE* file, int make_ready);
JXR_EXTERN int bs_seek(struct byte_stream* str, int offset, int whence);
JXR_EXTERN int bs_tell(struct byte_stream* str);
JXR_EXTERN int bs_read(struct byte_stream* str, unsigned char* buff, size_t size);
JXR_EXTERN int bs_write(struct byte_stream* str, const unsigned char* buff, size_t size);
JXR_EXTERN int bs_is_memory_stream(struct byte_stream* str);
JXR_EXTERN int bs_is_ready(struct byte_stream* str);
JXR_EXTERN void bs_copy(struct byte_stream* dest, const struct byte_stream* src);
JXR_EXTERN unsigned char bs_get_byte(struct byte_stream* str);
JXR_EXTERN int bs_feof(struct byte_stream* str);
JXR_EXTERN int bs_put_byte(struct byte_stream* str, unsigned char byte);
JXR_EXTERN void bs_flush(struct byte_stream* str);


#undef JXR_EXTERN