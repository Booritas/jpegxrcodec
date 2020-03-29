#include "jxrcodec.hpp"
#include <cstdint>
#include <vector>
#include "jxrcodec.hpp"
#include <cassert>
#include <stdexcept>

extern "C"
{
#include "codec/jpegxr.h"
#include "codec/file.h"
}

#define SAFE_FREE(h) {if(h)free(h); h = NULL;}
#define SAFE_CLOSE(h) {if(h)close_file(h); h = NULL;}
#define SAFE_JXR_DESTROY(h) {if(h) jxr_destroy(h); h = NULL;}

#ifdef _MSC_VER
/* MSVC doesn't have strcasecmp. Use _stricmp instead */
# define strcasecmp _stricmp
#else
# include <unistd.h>
#endif

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

int decompress_image(FILE *fd, jxr_container_t container, void *output_handle, jxr_image_t *pImage, 
			    unsigned char alpha, jxrflags* jflags)
{
    int rc, idx;
    *pImage = jxr_create_input();
    jxr_set_block_output(*pImage, write_file);
    //jxr_set_pixel_format(*pImage, jxrc_image_pixelformat(container, 0));
    jxr_set_user_data(*pImage, output_handle);
    jxr_set_PROFILE_IDC(*pImage, jflags->profile_idc);
    jxr_set_LEVEL_IDC(*pImage, jflags->level_idc);

    for (idx = 0 ; idx < jflags->nflags ; idx += 1) {
      if (strcmp(jflags->flags[idx],"SKIP_HP_DATA") == 0) {
	jxr_flag_SKIP_HP_DATA(*pImage, 1);
	continue;
      }
      if (strcmp(jflags->flags[idx],"SKIP_FLEX_DATA") == 0) {
	jxr_flag_SKIP_FLEX_DATA(*pImage, 1);
	continue;
      }
    }

    /*
    ** Start mod thor: This is the line-based decoding
    */
    if (jflags->line_mode) {
      rc = jxr_init_read_stripe_bitstream(*pImage,fd);
      if (rc >= 0) {
	/*
	** Now read lines one by another. The data arrives at
	** the usual "output hook" as always.
	*/
	do {
	  rc = jxr_read_stripe_bitstream(*pImage);
	} while(rc >= 0);

	if (rc == JXR_EC_DONE)
	  rc = 0; /* The regular return code */
      }
    } else {
      /* Process as an image bitstream. */
      rc = jxr_read_image_bitstream(*pImage, fd);
    }
    
    if (rc < 0) {
      switch (rc) {
      case JXR_EC_BADMAGIC:
	fprintf(stderr, "No valid magic number. Not an JPEG XR container or bitstream.\n");
	break;
      case JXR_EC_ERROR:
	fprintf(stderr, "Unspecified error.\n");
	break;
      case JXR_EC_FEATURE_NOT_IMPLEMENTED:
	fprintf(stderr,"A feature required to decode this bitstream is not implemented.\n");
	break;
      case JXR_EC_IO:
	fprintf(stderr,"I/O error reading bitstream.\n");
	break;
      case JXR_EC_BADFORMAT:
	fprintf(stderr,"File format invalid.\n");
	break;
      case JXR_EC_NOMEM:
	fprintf(stderr,"Out of memory.\n");
	break;
      default:
	fprintf(stderr, " Error %d reading image bitstream\n", rc);
	break;
      }
    } else {
      rc = jxr_test_LONG_WORD_FLAG(*pImage, jflags->long_word_flag_setting);
      if (rc < 0)
	fprintf(stderr, "LONG_WORD_FLAG condition was set incorrectly\n");
    }
    return rc;

}

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

void jpegxr_decompress(FILE* fd, FILE* output_file, uint8_t* buffer, uint32_t buffer_size)
{
    jxrflags jflags;
    ContainerKeeper container;
    jxr_container_t ifile = container.getContainer();
    const int padded_format = 1;
    std::vector<uint8_t> primary_buffer;

    int rc = jxr_read_image_container(ifile, fd);
    if (rc < 0) 
    {
        throw std::runtime_error("input image is not a jpegxr.");
    }
    unsigned long off = jxrc_image_offset(ifile, 0);
    rc = fseek(fd, off, SEEK_SET);
    assert(rc >= 0);
    if(jxrc_alpha_offset(ifile, 0))
    {
        throw std::runtime_error("Alpha channels are not supported!");
    }
    /* read optional IFD tags to make certain of conformance */
    char *document_name = 0, *image_description = 0, *equipment_make = 0, *equipment_model = 0, *page_name = 0;
    char *software_name_version = 0, *date_time = 0, *artist_name = 0, *host_computer = 0, *copyright_notice = 0;
    char** buffers[] = {
        &document_name, &image_description, &equipment_make, &equipment_model, &page_name,
        &software_name_version, &date_time, &artist_name, &host_computer, &copyright_notice
    };
    MemoryKeeper memKeeper(buffers, sizeof(buffers) / sizeof(buffers[0]));

    unsigned char profile, level;
    unsigned short page_number[2] = {0, 0}, color_space;
    unsigned long spatial_xfrm, image_type;
    float width_res, height_res;
    unsigned char image_band_present, alpha_band_present, buf[4];

    jxrc_document_name(ifile, 0, &document_name);
    jxrc_image_description(ifile, 0, &image_description);
    jxrc_equipment_make(ifile, 0, &equipment_make);
    jxrc_equipment_model(ifile, 0, &equipment_model);
    jxrc_page_name(ifile, 0, &page_name);
    jxrc_page_number(ifile, 0, page_number);
    jxrc_software_name_version(ifile, 0, &software_name_version);
    jxrc_date_time(ifile, 0, &date_time);
    jxrc_artist_name(ifile, 0, &artist_name);
    jxrc_host_computer(ifile, 0, &host_computer);
    jxrc_copyright_notice(ifile, 0, &copyright_notice);

    color_space = jxrc_color_space(ifile, 0);
    spatial_xfrm = jxrc_spatial_xfrm_primary(ifile, 0);
    image_type = jxrc_image_type(ifile, 0);
    jxrc_ptm_color_info(ifile, 0, buf);
    rc = jxrc_profile_level_container(ifile, 0, &profile, &level);
    if (rc < 0) {
        profile = 111;
        level = 255;
    }
    width_res = jxrc_width_resolution(ifile, 0);
    height_res = jxrc_height_resolution(ifile, 0);
    image_band_present = jxrc_image_band_presence(ifile, 0);
    alpha_band_present = jxrc_alpha_band_presence(ifile, 0);
    jxrc_padding_data(ifile, 0);

    void *output_handle(nullptr);
    MemoryKeeper primary_keeper((char**)&output_handle);
    //output_handle_primary = open_output_file(buffer, buffer_size, ifile, jflags.padded_format, 0);
    output_handle = open_output_file_h(output_file, ifile, jflags.padded_format, 0);
    /*Decode image */
    jxr_image_t image(nullptr), imageAlpha(nullptr);
    jxr_image_t* images[] = {&image, &imageAlpha};
    ImageKeeper image_keeper(images, sizeof(images)/sizeof(images[0]));
    rc = decompress_image(fd, ifile, output_handle, &image, 0, &jflags); 
    //SAFE_CLOSE(output_handle);
    if(rc < 0)
    {
        throw std::runtime_error("Failed to decompress image");
    }
}
