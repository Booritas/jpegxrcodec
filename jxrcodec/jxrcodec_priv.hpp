#pragma once
#include "codec/jxr_priv.h"

struct jxrflags
{
    jxrflags() :
        profile_idc(111),
        level_idc(255),
        nflags(0),
        line_mode(0),
        long_word_flag_setting(1),
        padded_format(0)
    {}
    int profile_idc;
    int level_idc;
    int line_mode;
    int nflags;
    int long_word_flag_setting;
    int padded_format;
    char* flags[128];
};

class ContainerKeeper
{
public:
    ContainerKeeper(){
        m_container = jxr_create_container();
    }
    ~ContainerKeeper(){
        if(m_container){
            jxr_destroy_container(m_container);
        }
    }
    jxr_container_t getContainer() const{
        return m_container;
    } 
private:
    jxr_container_t m_container;
};

class ImageKeeper
{
public:
    ImageKeeper(jxr_image_t** images, int32_t len)
    {
        m_images.assign(images, images+len);
    }
    ~ImageKeeper()
    {
        for(auto ptr : m_images){
            if (ptr){
                jxr_image_t image = *ptr;
                if(image){
                    jxr_destroy(image);
                }
            }
        }
    }
private:
    std::vector<jxr_image_t*> m_images;
};
class MemoryKeeper
{
public:
    MemoryKeeper(char*** buffers, int len){
        m_buffers.assign(buffers, buffers+len);
    }
    MemoryKeeper(char** buffer){
        m_buffers.push_back(buffer);
    }
    ~MemoryKeeper(){
        for(auto ptr : m_buffers){
            if (ptr){
                void* buffer = *ptr;
                if(buffer){
                    free(buffer);
                }
            }
        }
    }
private:
    std::vector<char**> m_buffers;
};
