#include "jxrcodec.hpp"
#include <cstdint>
#include <vector>
#include "jxrcodec.hpp"
#include <cassert>
#include <stdexcept>
#include "codec/jpegxr.h"
#include "jxrcodec_priv.hpp"

extern "C"
{
#include "codec/file.h"
}

int decompress_image(byte_stream* bs, jxr_container_t container, void *output_handle, jxr_image_t *pImage, 
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
      rc = jxr_init_read_stripe_bitstream(*pImage,bs);
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
      rc = jxr_read_image_bitstream(*pImage, bs);
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


void jpegxr_decompress(byte_stream& bs, uint8_t* output_buffer, uint32_t buffer_size)
{
    jxrflags jflags;
    ContainerKeeper container;
    jxr_container_t ifile = container.getContainer();
    const int padded_format = 1;
    std::vector<uint8_t> primary_buffer;

    int rc = jxr_read_image_container(ifile, &bs);
    if (rc < 0) 
    {
        throw std::runtime_error("input image is not a jpegxr.");
    }
    unsigned long off = jxrc_image_offset(ifile, 0);
    rc = bs_seek(&bs, off, SEEK_SET);
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
    output_handle = open_output_file_mem(output_buffer, buffer_size, ifile, jflags.padded_format, 0);
    /*Decode image */
    jxr_image_t image(nullptr), imageAlpha(nullptr);
    jxr_image_t* images[] = {&image, &imageAlpha};
    ImageKeeper image_keeper(images, sizeof(images)/sizeof(images[0]));
    rc = decompress_image(&bs, ifile, output_handle, &image, 0, &jflags); 
    if(rc < 0)
    {
        throw std::runtime_error("Failed to decompress image");
    }
}

void jpegxr_decompress(uint8_t *input_buffer, uint32_t input_buffer_size, uint8_t* output_buffer, uint32_t buffer_size)
{
    byte_stream bs;
    bs_init_mem(&bs, input_buffer, input_buffer_size, 1);
    jpegxr_decompress(bs,  output_buffer, buffer_size);
}

void jpegxr_decompress(FILE* input_file, uint8_t* output_buffer, uint32_t buffer_size)
{
    byte_stream bs;
    bs_init_file(&bs, input_file, 1);
    jpegxr_decompress(bs,  output_buffer, buffer_size);
}