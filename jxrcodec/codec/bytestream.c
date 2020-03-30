#include "bytestream.h"
#include <string.h>

void bs_init_mem(struct byte_stream* bs, unsigned char* buffer, size_t size, int make_ready)
{
    bs->buffer_pos = NULL;
    bs->buffer_pos2 = buffer;
    bs->buffer_begin = buffer;
    bs->buffer_end = buffer + size;
    bs->file = NULL;
    bs->file2 = NULL;
    if(make_ready!=0)
        bs_make_ready(bs);
}

void bs_make_ready(struct byte_stream* bs)
{
    if(bs_is_memory_stream(bs))
    {
        bs->buffer_pos = bs->buffer_pos2;
    }
    else
    {
        bs->file = bs->file2;
    }
}

void bs_make_unready(struct byte_stream* bs)
{
    if(bs_is_memory_stream(bs))
    {
        bs->buffer_pos2 = bs->buffer_pos;
        bs->buffer_pos = NULL;
    }
    else
    {
        bs->file2 = bs->file;
        bs->file = NULL;
    }
}

void bs_init_file(struct byte_stream* bs, FILE* file, int make_ready)
{
    bs->buffer_pos = NULL;
    bs->buffer_begin = NULL;
    bs->buffer_end = NULL;
    bs->file = NULL;
    bs->file2 = file;
    if(make_ready!=0)
        bs_make_ready(bs);
}

int bs_seek(struct byte_stream* bs, int offset, int whence)
{
    int ret = 1;
    if(bs_is_memory_stream(bs))
    {
        int old_offset = bs_tell(bs);
        int new_offset = 0;
        int buffer_size = (int)(bs->buffer_end - bs->buffer_begin);
        switch(whence)
        {
        case SEEK_SET:
            new_offset = offset;
            break;
        case SEEK_CUR:
            new_offset = old_offset + offset;
            break;
        case SEEK_END:
            new_offset = buffer_size + offset;
            break;
        }
        if(new_offset<0 || new_offset>=buffer_size)
        {
            ret = 0;
        }
        else
        {
            bs->buffer_pos = bs->buffer_begin + new_offset;
        }
    }
    else
    {
        int rc = fseek(bs->file, offset, whence);
        if (rc!=0)
            ret = 0;
    }
    return ret;
}

int bs_tell(struct byte_stream* bs)
{
    if(bs_is_memory_stream(bs))
    {
        return (int)(bs->buffer_pos - bs->buffer_begin);
    }
    else
    {
        return ftell(bs->file);
    }
}

int bs_read(struct byte_stream* bs, unsigned char* buff, size_t size)
{
    int count = 0;
    if(bs_is_memory_stream(bs))
    {
        unsigned char* new_pos = bs->buffer_pos + size;
        size_t new_size = size;
        if(new_pos>bs->buffer_end)
        {
            new_size = bs->buffer_end - bs->buffer_pos;
            new_pos = bs->buffer_pos + new_size;
        }
        if(new_size>0)
        {
            memcpy(buff, bs->buffer_pos, new_size);
            bs->buffer_pos = new_pos;
        }
        count = (int)new_size;
    }
    else
    {
        count = (int)fread(buff, 1, size, bs->file);
    }
    return count;
}

unsigned char bs_get_byte(struct byte_stream* bs)
{
    unsigned char ret = EOF;
    
    if(bs_is_memory_stream(bs))
    {
        if(bs->buffer_pos < bs->buffer_end)
        {
            ret = *(bs->buffer_pos);
            bs->buffer_pos += 1;
        }
    }
    else
    {
        ret = fgetc(bs->file);
    }
    return ret;
}

int bs_put_byte(struct byte_stream* bs, unsigned char byte)
{
    int ret = 0;
    if(bs_is_memory_stream(bs))
    {
        if(bs->buffer_pos < bs->buffer_end)
        {
            *(bs->buffer_pos) = byte;
            bs->buffer_pos += 1;
            ret = byte;
        }
    }
    else
    {
        ret = fputc(byte, bs->file);
    }
    return ret;
}

void bs_flush(struct byte_stream* str)
{
    if(!bs_is_memory_stream(str))
    {
        if(str->file)
            fflush(str->file);
    }
}

int bs_feof(struct byte_stream* str)
{
    int ret = 0;
    if(bs_is_memory_stream(str))
    {
        if(str->buffer_pos>=str->buffer_end)
            ret = 1;
    }
    else
    {
        ret = feof(str->file);
    }

    return ret;
}


int bs_write(struct byte_stream* bs, const unsigned char* buff, size_t size)
{
    int count = 0;
    if(bs_is_memory_stream(bs))
    {
        unsigned char* new_pos = bs->buffer_pos + size;
        size_t new_size = size;
        if(new_pos>bs->buffer_end)
        {
            new_size = (size_t)(bs->buffer_end - bs->buffer_pos);
            new_pos = bs->buffer_pos + new_size;
        }
        if(new_size>0)
        {
            memcpy(bs->buffer_pos, buff, new_size);
            bs->buffer_pos = new_pos;
        }
        count = (int)new_size;
    }
    else
    {
        count = (int)fwrite(buff, 1, size, bs->file);
    }
    return count;
}

int bs_is_memory_stream(struct byte_stream* bs)
{
    int mem = 1;
    if(bs->buffer_begin==NULL)
        mem = 0;
    return mem;
}

int bs_is_ready(struct byte_stream* str)
{
    int ready = 0;
    if(bs_is_memory_stream(str))
    {
        if(str->buffer_pos!=0)
            ready = 1;
    }
    else
    {
        if(str->file!=NULL)
            ready = 1;
    }
    return ready;
}

void bs_copy(struct byte_stream* dest, const struct byte_stream* src)
{
    *dest = *src;
}
