/*
**
** $Id: jpegxr.c,v 1.47 2014/10/23 15:52:14 thor Exp $
**
** This is the main program
**
*/

/*************************************************************************
*
* This software module was originally contributed by Microsoft
* Corporation in the course of development of the
* ITU-T T.832 | ISO/IEC 29199-2 ("JPEG XR") format standard for
* reference purposes and its performance may not have been optimized.
*
* This software module is an implementation of one or more
* tools as specified by the JPEG XR standard.
*
* ITU/ISO/IEC give You a royalty-free, worldwide, non-exclusive
* copyright license to copy, distribute, and make derivative works
* of this software module or modifications thereof for use in
* products claiming conformance to the JPEG XR standard as
* specified by ITU-T T.832 | ISO/IEC 29199-2.
*
* ITU/ISO/IEC give users the same free license to this software
* module or modifications thereof for research purposes and further
* ITU/ISO/IEC standardization.
*
* Those intending to use this software module in products are advised
* that its use may infringe existing patents. ITU/ISO/IEC have no
* liability for use of this software module or modifications thereof.
*
* Copyright is not released for products that do not conform to
* to the JPEG XR standard as specified by ITU-T T.832 |
* ISO/IEC 29199-2.
*
******** Section to be removed when the standard is published ************
*
* Assurance that the contributed software module can be used
* (1) in the ITU-T "T.JXR" | ISO/IEC 29199 ("JPEG XR") standard once the
* standard has been adopted; and
* (2) to develop the JPEG XR standard:
*
* Microsoft Corporation and any subsequent contributors to the development
* of this software grant ITU/ISO/IEC all rights necessary to include
* the originally developed software module or modifications thereof in the
* JPEG XR standard and to permit ITU/ISO/IEC to offer such a royalty-free,
* worldwide, non-exclusive copyright license to copy, distribute, and make
* derivative works of this software module or modifications thereof for
* use in products claiming conformance to the JPEG XR standard as
* specified by ITU-T T.832 | ISO/IEC 29199-2, and to the extent that
* such originally developed software module or portions of it are included
* in an ITU/ISO/IEC standard. To the extent that the original contributors
* may own patent rights that would be required to make, use, or sell the
* originally developed software module or portions thereof included in the
* ITU/ISO/IEC standard in a conforming product, the contributors will
* assure ITU/ISO/IEC that they are willing to negotiate licenses under
* reasonable and non-discriminatory terms and conditions with
* applicants throughout the world and in accordance with their patent
* rights declarations made to ITU/ISO/IEC (if any).
*
* Microsoft, any subsequent contributors, and ITU/ISO/IEC additionally
* gives You a free license to this software module or modifications
* thereof for the sole purpose of developing the JPEG XR standard.
*
******** end of section to be removed when the standard is published *****
*
* Microsoft Corporation retains full right to modify and use the code
* for its own purpose, to assign or donate the code to a third party,
* and to inhibit third parties from using the code for products that
* do not conform to the JPEG XR standard as specified by ITU-T T.832 |
* ISO/IEC 29199-2.
*
* This copyright notice must be included in all copies or derivative
* works.
*
* Copyright (c) ITU-T/ISO/IEC 2008, 2009.
***********************************************************************/

#ifdef _MSC_VER
#pragma comment (user,"$Id: jpegxr.c,v 1.47 2014/10/23 15:52:14 thor Exp $")
#endif

# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <ctype.h>
# include "jpegxr.h"
# include "file.h"
# include <assert.h>

/* MSC doesn't include getopt, so if compiling with MSC then get it
from our local copy. Otherwise use the C library version. */
#ifdef _MSC_VER
# include "getopt.h"
/* MSVC doesn't have strcasecmp. Use _stricmp instead */
# define strcasecmp _stricmp
#else
# include <unistd.h>
#endif

char*path_out = 0;
char*path_debug = 0;
char*quant_uniform = 0;

char*flags[128];
int nflags = 0;

static jxr_bands_present_t bands_present = JXR_BP_ALL;
static int trim_flexbits = 0;
static int overlap_filter = 1;
static jxr_color_fmt_t internal_color_fmt = JXR_YUV444;
static int disable_tile_overlap = 0;
static int frequency_mode_flag = 0;
static int profile_idc = 111;
static int level_idc = 255;
static int long_word_flag_setting = 1;
static int alpha_mode = 2;
static int padded_format = 0;
static int bgr_flag = 0; /* if set, input is in BGR rather than RGB */
static unsigned char window_params[5] = {0,0,0,0,0};
static jxrc_t_pixelFormat pxl_fmt;

/* Set this flag if the encoder should try to use the USE_DC_QP and
USE_LP_QP flags when it can. */
static int use_dc_qp_flag = 0;

# define MAX_CHANNELS 16
static unsigned char quant_per_channel[MAX_CHANNELS];
static unsigned quant_per_channel_count = 0;

# define MAX_TILES 4096
static unsigned tile_rows = 1;
static unsigned tile_columns = 1;
static unsigned tile_width_in_MB[MAX_TILES * 2] = {0};
static unsigned tile_height_in_MB[MAX_TILES * 2] = {0};
static unsigned chroma_x_centering = 0;
static unsigned chroma_y_centering = 0;

static unsigned char shift_bits = 0;
static unsigned char len_mantissa = 10;
static char exp_bias = 4;

static raw_info raw_info_t;

/*
** Added by thor April 2nd 2010: Line based mode.
*/
static int line_mode = 0;

/*
** Added by thor February 2012: ISO box based wrapper.
*/
static int box_wrapper = 0;

/*
** Added by thor March 2012: Alternate encoding, e.g.
** switches from CMYK to CMYKDirect for tiff input.
*/
static int alternate_format = 0;

/*
** Prototypes
*/

static int main_compress(const char*path);
static int main_decompress(const char*path);

/* This function parses a qp input file and returns an array of
jxr_tile_qp with the results. The parser needs to know the tile
dimensions of the image in order to get the right number of
tiles. */
extern int qp_parse_file(FILE*fd, jxr_image_t image);
/* Path to the input quant file. */
static const char*quant_path = 0;

int main(int argc, char*argv[])
{
    int compress_flag = 0;
    int flag;
    int derive_flag = 0;
    char*cp_temp;
    int idx;

    /* initialize raw input */
    raw_info_t.is_raw = 0;
    raw_info_t.raw_width = 0;
    raw_info_t.raw_height = 0;
    raw_info_t.raw_format = 3;
    raw_info_t.raw_bpc = 8;

    while ((flag = getopt(argc, argv, "b:cxyphmwdgrAa:D:f:F:l:o:U:C:R:P:L:q:Q:X:W:H:M:B:s:S:")) != -1)  {
      switch (flag) {
      case 'b': /* Bands present */
	if (strcasecmp(optarg,"ALL") == 0)
	  bands_present = JXR_BP_ALL;
	else if (strcasecmp(optarg,"NOFLEXBITS") == 0)
	  bands_present = JXR_BP_NOFLEXBITS;
	else if (strcasecmp(optarg,"NOHIGHPASS") == 0)
	  bands_present = JXR_BP_NOHIGHPASS;
	else if (strcasecmp(optarg,"DCONLY") == 0)
	  bands_present = JXR_BP_DCONLY;
	else {
	  fprintf(stderr, "Invalid bands flag: %s\n", optarg);
	  fprintf(stderr, "Should be one of: ALL, NOFLEXBITS, NOHIGHPASS, DCONLY\n");
	  return 1;
	}
	break;
	
      case 'c':
	compress_flag = 1;
	break;
	
      case 'y':
	line_mode = 1;
	break;

      case 'x':
	box_wrapper = 1;
	break;

      case 'd':
	derive_flag = 1;
	break;
	
      case 'D':
	path_debug = optarg;
	break;
	
      case 'p':
	padded_format = 1;
	break;
	
      case 'g':
	bgr_flag = 1;
	break;
	
      case 'f':
	if (strcmp(optarg, "USE_DC_QP") == 0) {
	  use_dc_qp_flag = 1;
	} else if (strcmp(optarg,"YUV444") == 0) {
	  internal_color_fmt = JXR_YUV444;
	} else if (strcmp(optarg,"YUV422") == 0) {
	  internal_color_fmt = JXR_YUV422;
	} else if (strcmp(optarg,"YUV420") == 0) {
	  internal_color_fmt = JXR_YUV420;
	} else if (strcmp(optarg,"YOnly") == 0) {
	  internal_color_fmt = JXR_YONLY;
	} else {
	  assert(!"Invalid color format specified");
	}
	break;
	
      case 'F':
	trim_flexbits = strtoul(optarg, 0, 10);
	if (trim_flexbits > 15) {
	  trim_flexbits = 15;
	}
	break;
	
      case 'h':
	disable_tile_overlap = 1;
	break;
	
      case 'm':
	frequency_mode_flag = 1;
	break;
	
      case 'w':
	long_word_flag_setting = 0;
	break;
	
      case 'l':
	overlap_filter = strtoul(optarg, 0, 10);
	if (overlap_filter >= 3) {
	  fprintf(stderr, "Invalid overlap filter flag. Got %d\n", overlap_filter);
	  return 1;
	}
	break;
	
      case 'X':
	flags[nflags] = optarg;
	nflags += 1;
	break;
	
      case 'o':
	path_out = optarg;
	break;
	
      case 's':
	cp_temp = optarg;
	window_params[0] = 1;
	unsigned read_value;
	for (idx = 0 ; idx < 4 ; idx += 1) {
	  read_value = (unsigned) strtoul(cp_temp, &cp_temp, 10);
	  assert(read_value >= 0 && read_value < 64);
	  window_params[idx + 1] = (unsigned char) read_value;
	  if (*cp_temp == ':')
	    cp_temp += 1;
	}
	break;

      case 'S': // chroma sample position
	cp_temp = optarg;
	chroma_x_centering = strtoul(cp_temp, &cp_temp, 10);
	if (*cp_temp == ':') {
	  cp_temp += 1;
	  chroma_y_centering = strtoul(cp_temp,&cp_temp,10);
	}
	break;
	
      case 'U':
	cp_temp = optarg;
	tile_rows = (unsigned) strtoul(cp_temp, &cp_temp, 10);
	if (tile_rows > MAX_TILES)
	  tile_rows = MAX_TILES;
	if (*cp_temp == ':') {
	  cp_temp += 1;
	  tile_columns = (unsigned) strtoul(cp_temp, &cp_temp, 10);
	  if (tile_columns > MAX_TILES)
	    tile_columns = MAX_TILES;
	} else {
	  tile_columns = tile_rows;
	}
	
	break;
	
      case 'A':
	alternate_format = 1;
	break;

      case 'a':
	alpha_mode = strtoul(optarg, 0, 10);
	if(alpha_mode <0 || alpha_mode > 2)
	  assert(!"invalid alpha mode");
	break;
	
      case 'C':
	cp_temp = optarg;
	for (tile_columns = 0 ; tile_columns < MAX_TILES ; tile_columns++) {
	  tile_width_in_MB[tile_columns] = (unsigned) strtoul(cp_temp, &cp_temp, 10);
	  if (*cp_temp == ':')
	    cp_temp += 1;
	  else
	    break;
	}
	tile_columns++;
	break;
	
      case 'R':
	cp_temp = optarg;
	for (tile_rows = 0 ; tile_rows < MAX_TILES ; tile_rows++) {
	  tile_height_in_MB[tile_rows] = (unsigned) strtoul(cp_temp, &cp_temp, 10);
	  if (*cp_temp == ':')
	    cp_temp += 1;
	  else
	    break;
	}
	tile_rows++;
	break;
	
	
      case 'q': { /* Uniform quantization */
	quant_uniform = optarg;
	quant_per_channel_count = 0;
	int last_q = 0;
	cp_temp = optarg;
	for (idx = 0 ; idx < MAX_CHANNELS ; idx += 1) {
	  if (*cp_temp == 0) {
	    quant_per_channel[idx] = quant_per_channel[last_q];
	    continue;
	  }
	  last_q = idx;
	  quant_per_channel_count += 1;
	  quant_per_channel[idx] = (unsigned char) strtoul(cp_temp, &cp_temp, 10);
	  if (*cp_temp == ':')
	    cp_temp += 1;
	}
	break;
      }
	
      case 'Q':
	quant_path = optarg;
	break;
	
      case 'P':
	profile_idc = strtoul(optarg, 0, 10);
	if (profile_idc > 255) {
	  profile_idc = 255;
	}
	if (profile_idc < 0) {
	  profile_idc = 0;
	}
	break;
	
      case 'L':
	level_idc = strtoul(optarg, 0, 10);
	if (level_idc > 255) {
	  level_idc = 255;
	}
	if (level_idc < 0) {
	  level_idc = 0;
	}
	break;

      case 'r':
	raw_info_t.is_raw = 1; 
	break; 
	
      case 'W':
	raw_info_t.raw_width = (unsigned int)atoi(optarg);
	break;
	
      case 'H':
	raw_info_t.raw_height = (unsigned int)atoi(optarg);
	break;
	
      case 'M':
	raw_info_t.raw_format = (unsigned char)atoi(optarg);
	if ((raw_info_t.raw_format < 3) || (raw_info_t.raw_format > 34)) {
	  fprintf(stderr, "Invalid raw format.\n");
	  return -1;
	}                
	break; 
	
      case 'B':
	raw_info_t.raw_bpc = (unsigned char)atoi(optarg);
	if ((8 != raw_info_t.raw_bpc) && (10 != raw_info_t.raw_bpc) && (16 != raw_info_t.raw_bpc)) {
	  fprintf(stderr, "Invalid bit/channel.\n");
	  return -1;
	}                
	break; 
	
      case '?':
	fprintf(stderr, "Abort from bad flag.\n");
	return -1;
      }
    }

    if (optind >= argc) {
      fprintf(stderr,"Usage: %s <flags> <input_file.jxr>\n"
	      "  DECODER FLAGS:\n"
	      "    [-o <path>] [-w] [-P 44|55|66|111] [-L 4|8|16|32|64|128|255]\n"
	      "\n"
	      "\t-o: selects output file name (.raw/.tif/.pnm/.rgbe)\n"
	      "\t    (PNM output can be used only for 24bpp RGB and 8bpp gray Output)\n"
	      "\t    (TIF output can be used for all formats)\n"
	      "\t    (RAW output can be used for all formats)\n"
	      "\t-y: select stripe by stripe decoding or encoding\n"
	      "\t-w: tests whether LONG_WORD_FLAG needs to be equal to TRUE\n"
	      "\t     (will still decode the image)\n"
	      "\t-P: selects the maximum accepted profile value\n"
	      "\t     (44:Sub-Baseline|55:Baseline|66:Main|111:Advanced)\n"
	      "\t-L: selects the maximum accepted level value\n"
	      "\t     (4|8|16|32|64|128)\n"
	      "\t-p: tries to reproduce the padding channel on output. Otherwise,\n"
	      "\t     padding data will not be written\n"
	      "\n"
	      "  ENCODER FLAGS: (Temporary (.tmp) files may be used in encoding)\n"
	      "    -c [-o <path>] [-b ALL|NOFLEXBITS|NOHIGHPASS|DCONLY] [-a 0|1|2] [-p]\n"
	      "    [-f YUV444|YUV422|YUV420|YOnly] [-F bits] [-h] [-m] [-l 0|1|2]\n"
	      "    [-q q1[:q2[:q3]]] [-Q <path>] [-d] [-w] [-U rows:columns]\n"
	      "    [-C width1[:width2>[:width3...]]] [-R height1[:height2[:height3...]]]\n"
	      "    [-P 44|55|66|111] [-L 4|8|16|32|64|128|255] [-s top|left|bottom|right]\n"
	      "    [-r -W width -H height -M 3|4|...|34 [-B 8|16]]\n"
	      "\n"
	      "\t-c: selects encoding instead of decoding\n"
	      "\t     this flag is necessary for encoding\n"
	      "\t-o: selects the output file name (.jxr)\n"
	      "\t-b: selects the bands to encode\n"
	      "\t     (ALL<Default>|NOFLEXBITS|NOHIGHPASS|DCONLY)\n"            
	      "\t-a: selects encoder alpha mode\n"
	      "\t     (0: no alpha|1:interleaved alpha|2:separate alpha) \n"
	      "\t     Default: For tif input files, based on the information in the\n"
	      "\t     PhotometricInterpretation and SamplesPerPixel tags in the container,\n"
	      "\t     the encoder chooses an input pixel format. If the number\n"
	      "\t     of components is 4 and photometric is 2, RGBA input is inferred and \n"
	      "\t     the encoder assumes the presence of an alpha channel while encoding.\n"
            "\t     If the number of components is 5 and photometric is 5, CMYKA input is\n"
	      "\t     inferred. In both these cases, the encoder infers a pixel format with\n"
	      "\t     an alpha channel. In such cases, the default alpha encoder mode is 2.\n"
	      "\t     For raw input files, when the -M parameter specified by the user is\n"
	      "\t     9, 10, 11, 12, 13, 14, 23, 24, 25, 26 or 28,\n"
	      "\t     the default alpha encoder mode is 2\n"
	      "\t     In all other cases, the default alpha encoder mode is 0.\n"
	      "\t-p:  selects an input pixel format with a padding channel\n"
	      "\t     With tif input, when the encoder infers that the input file has an\n"
	      "\t     alpha channel (see explanation for -a), this flag causes the encoder\n"
	      "\t     to treat the alpha channel as a padding channel instead\n"
	      "\t-g:  Identifies that red and blue shall be swapped in 16bppBGR555,16bppBGR565\n"
	      "\t     or 32bppBGR101010.\n"
	      "\t     NOTE: This only works for 16bppBGR555,16bppBGR565 and 32bppBGR101010\n"
	      "\t           inputs. It changes the color transform such that red and blue are\n"
	      "\t           interchanged. Some legacy implementations might require this to\n"
	      "\t           decode images correctly in these formats *only*.\n"
	      "\t-f: selects the internal color format\n"
	      "\t     (YUV444<Default>|YUV422|YUV420|YOnly)\n"
	      "\t-F: selects the number of flexbits to trim\n"
	      "\t     (0<default> - 15)\n"
	      "\t-h: selects hard tile boundaries\n"
	      "\t     (soft tile boundaries by default)\n"
	      "\t-m: encode in frequency order codestream\n"
	      "\t     (spatial order by default)\n"
	      "\t-l: selects the overlapped block filtering\n"
	      "\t     (0:off|1:HP only<Default>|2:all)\n"
	      "\n"
	      "\t-q: sets the quantization values separately, or one per band\n"
	      "\t     (0<default, lossless> - 255)\n"
	      "\t-Q: specifies a file containing detailed quantization information\n"
	      "\t     See sample.qp\n"
	      "\t-d: selects quantization for U/V channels derived from Y channel\n"
	      "\n"
	      "\t-U: selects uniform tile sizes\n"
	      "\t-C: selects the number of tile columns and the width of each\n"
	      "\t-R: selects the number of tile rows and the height of each\n"
	      "\n"
	      "\t-w: sets LONG_WORD_FLAG equal to FALSE\n"
	      "\t-P: selects the profile value\n"
	      "\t     (44:Sub-Baseline|55:Baseline|66:Main|111:Advanced)\n"
	      "\t-L: selects the level value\n"
	      "\t     (4|8|16|32|64|128)\n"
	      "\t-s: sets the top, left, bottom, and right margins\n"
	      "\t     (top:left:bottom:right)\n"
	      "\t-S: selects chroma x and y centering, requires two numbers\n"
	      "\t     separated by colon, i.e. 'x:y'\n"
	      "\t-x: encodes images in the ISO box-based file format instead\n"
	      "\t     of the TIFF tag format\n"
	      "\t-A: selects alternative format in case several interpretations of\n"
	      "\t     the input file are possible, e.g. switches from CMYK to CMYKDirect\n"
	      "\t-r: selects encoding with RAW images\n"
	      "\t     must also specify -W, -H and -M, optional -B\n"
	      "\t-W: RAW image width when encoding with RAW images\n"
	      "\t-H: RAW image height when encoding with RAW images\n"
	      "\t-M: RAW image format when encoding with RAW images\n"
	      "\t    3: 3-channel\n"
	      "\t    4: 4-channel\n"
	      "\t    5: 5-channel\n"
	      "\t    6: 6-channel\n"
	      "\t    7: 7-channel\n"
	      "\t    8: 8-channel\n"
	      "\t    9: 3-channel Alpha\n"
	      "\t    10: 4-channel Alpha\n"
	      "\t    11: 5-channel Alpha\n"
	      "\t    12: 6-channel Alpha\n"
	      "\t    13: 7-channel Alpha\n"
	      "\t    14: 8-channel Alpha\n"
	      "\t    15: 32bppRGBE\n"
	      "\t    16: 16bppRGB555\n"
	      "\t    17: 16bppRGB565\n"
	      "\t    18: 32bppRGB101010\n"
	      "\t    19: YCC420\n"
	      "\t    20: YCC422\n"
	      "\t    21: YCC444\n"
	      "\t    22: YCC444 Fixed Point\n"
	      "\t    23: YCC420 Alpha\n"
	      "\t    24: YCC422 Alpha\n"
	      "\t    25: YCC444 Alpha\n"
	      "\t    26: YCC444 Fixed Point Alpha\n"
	      "\t    27: CMYKDIRECT\n"
	      "\t    28: CMYKDIRECT Alpha\n"
	      "\t    29: 24bppRGB\n"
	      "\t    30: 32bppRGB\n"
	      "\t    31: 32bppRGBA\n"
	      "\t    32: 32bppPRGBA\n"
	      "\t    33: 64bppPRGBA\n"
	      "\t    34: 128bppPRGBAFloat\n"
	      "\t-B: RAW image bit/channel when encoding with RAW images\n"
	      "\t    8: 8-bit/channel (default)\n"
	      "\t    10: 10-bit/channel\n"
	      "\t    16: 16-bit/channel\n"
	      "\n",
	      argv[0]);
      return 1;
    }

    /*
    ** start mod thor: Derived quantization as in the DPK
    */
    if (derive_flag) {
      int idx = quant_per_channel[0];
      int idxuv = idx;
      switch (internal_color_fmt) {
      case JXR_YUV444:
	if (idx < 16) {
	  idxuv = idx * 2;
	} else if (idx <= 48) {
	  idxuv = idx + 18;
	} else {
	  idxuv = idx + 18 + 2;
	}
	quant_per_channel_count = 3;
	break;
      case JXR_YUV422:
	if (idx < 16) {
	  idxuv = idx + ((idx + 1) >> 1);
	} else if (idx <= 48) {
	  idxuv = idx + 8;
	} else {
	  idxuv = idx + 8 + 2;
	}
	quant_per_channel_count = 3;
	break;
      case JXR_YUV420:
	if (idx < 16) {
	  idxuv = idx + ((idx + 2) >> 2);
	} else if (idx <= 48) {
	  idxuv = idx + 4;
	} else {
	  idxuv = idx + 4 + 2;
	}
	quant_per_channel_count = 3;
	break;
      default:
	idxuv = idx;
	break;
      }
      for(idx = 1;idx < MAX_CHANNELS;idx++) {
	quant_per_channel[idx] = idxuv;
      }
    }
    /*
    ** End mod thor
    */

    if(disable_tile_overlap)
        assert(compress_flag);

    if (frequency_mode_flag)
        assert(compress_flag);

    if (compress_flag)
        return main_compress(argv[optind]);
    else
        return main_decompress(argv[optind]);
}

int setup_image_params(jxr_image_t *ptr_image, void *input_handle, int alpha_mode, int alpha_plane)
{
    int rc = 0;

    int wid, hei, ncomp, bpi;
    short sf, photometric;
    int padBytes;
    int ycc;
    int premultiplied = 0;
    int alpha = 0;
    
    get_file_parameters(input_handle, &wid, &hei, &ncomp, &bpi, &sf, &photometric, &padBytes,&ycc,&premultiplied,&alpha);

    wid -= (window_params[2] + window_params[4]);
    assert(wid > 0);
    hei -= (window_params[1] + window_params[3]);
    assert(hei > 0);

    /* Create a stub image. */
    jxr_image_t image = jxr_create_image(wid, hei, window_params);
    *ptr_image = image;

    jxr_set_CHROMA_CENTERING(image,chroma_x_centering,chroma_y_centering);
    
    if (!raw_info_t.is_raw) {
      int rawcomp = ncomp + padBytes - ((alpha)?(1):(0));
      /* Guess a color format from the number of components. */
      if (!alpha_plane) {
	if (photometric == -1 && rawcomp >= 3 && rawcomp <= 8 && (bpi == 8 || bpi == 16)) { 
	  jxr_set_INTERNAL_CLR_FMT(image, JXR_NCOMPONENT, rawcomp, alpha);
	  jxr_set_OUTPUT_CLR_FMT(image, JXR_OCF_NCOMPONENT);
	} else {
	  switch (ncomp + padBytes) {
	  case 1:
	    jxr_set_INTERNAL_CLR_FMT(image, JXR_YONLY, 1, alpha);
	    jxr_set_OUTPUT_CLR_FMT(image, JXR_OCF_YONLY);
	    break;
	  case 3:
	    if (photometric == 6) {
	      switch(ycc) {
	      case 1:
		jxr_set_OUTPUT_CLR_FMT(image, JXR_OCF_YUV420);
		jxr_set_INTERNAL_CLR_FMT(image, JXR_YUV420, 3,0);
		break;
	      case 2:
		jxr_set_OUTPUT_CLR_FMT(image, JXR_OCF_YUV422);
		jxr_set_INTERNAL_CLR_FMT(image, JXR_YUV422, 3,0);
		break;
	      case 3:
		jxr_set_OUTPUT_CLR_FMT(image, JXR_OCF_YUV444);
		/* FIX THOR: YUV444 also allows other internal color formats */
		jxr_set_INTERNAL_CLR_FMT(image, internal_color_fmt, 3,0);
		break;
	      }
	    } else {
	      jxr_set_INTERNAL_CLR_FMT(image, internal_color_fmt, 3,0);
	      jxr_set_OUTPUT_CLR_FMT(image, JXR_OCF_RGB);
	    }
	    break;
	  case 4:
	    if (photometric == 5) {
	      jxr_set_INTERNAL_CLR_FMT(image, JXR_YUVK, 4,0);
	      if (alternate_format || raw_info_t.raw_format == 27 || raw_info_t.raw_format == 28) {
		jxr_set_OUTPUT_CLR_FMT(image, JXR_OCF_CMYKDIRECT);
	      } else {
		jxr_set_OUTPUT_CLR_FMT(image, JXR_OCF_CMYK);
	      }
	    } else if (photometric == 2) { /* RGB_NULL or RGB_alpha */
	      jxr_set_INTERNAL_CLR_FMT(image, internal_color_fmt, 3, 1);
	      jxr_set_OUTPUT_CLR_FMT(image, JXR_OCF_RGB);
	    } else if (photometric == 6) {
	      switch(ycc) {
	      case 1:
		jxr_set_OUTPUT_CLR_FMT(image, JXR_OCF_YUV420);
		jxr_set_INTERNAL_CLR_FMT(image, JXR_YUV420, 3, 1);
		break;
	      case 2:
		jxr_set_OUTPUT_CLR_FMT(image, JXR_OCF_YUV422);
		jxr_set_INTERNAL_CLR_FMT(image, JXR_YUV422, 3, 1);
		break;
	      case 3:
		jxr_set_OUTPUT_CLR_FMT(image, JXR_OCF_YUV444);
		/* FIX THOR: YUV444 also allows other internal color formats */
		jxr_set_INTERNAL_CLR_FMT(image, internal_color_fmt, 3, 1);
		break;
	      }
	    } else if (photometric == -1) { // RGBE
	      jxr_set_INTERNAL_CLR_FMT(image, JXR_YUV444, 3, 0);
	      jxr_set_OUTPUT_CLR_FMT(image, JXR_OCF_RGBE);
	      jxr_set_OUTPUT_BITDEPTH(image, JXR_BD8);   
	    }
	    break;
	  case 5:
	    if (photometric == 5){ /* CMYKA */
	      jxr_set_INTERNAL_CLR_FMT(image, JXR_YUVK, 4, 1);
	      /* The raw format is 3 in case it has never been defined before, i.e. the user
		 gave no further instructions
	      */
	      if (alternate_format || raw_info_t.raw_format == 27 || raw_info_t.raw_format == 28) {
		jxr_set_OUTPUT_CLR_FMT(image, JXR_OCF_CMYKDIRECT);
	      } else {
		jxr_set_OUTPUT_CLR_FMT(image, JXR_OCF_CMYK);
	      }
	    }
	    break;
	  default:
	    jxr_set_INTERNAL_CLR_FMT(image, JXR_NCOMPONENT, ncomp - alpha, alpha);
	    jxr_set_OUTPUT_CLR_FMT(image, JXR_OCF_NCOMPONENT);
	    break;
	  }
	}
      } else {
	jxr_set_INTERNAL_CLR_FMT(image, JXR_YONLY, 1, 0);
	jxr_set_OUTPUT_CLR_FMT(image, JXR_OCF_YONLY);            
      }
      
      switch (bpi) {
      case 1:
	if (photometric) /*white is 1 */
	  jxr_set_OUTPUT_BITDEPTH(image, JXR_BD1WHITE1);
	else 
	  jxr_set_OUTPUT_BITDEPTH(image, JXR_BD1BLACK1);
	break;
      case 8:
	jxr_set_OUTPUT_BITDEPTH(image, JXR_BD8);
	break;
      case 16:
	if (sf == 1) {
	  jxr_set_OUTPUT_BITDEPTH(image, JXR_BD16);
	  jxr_set_SHIFT_BITS(image, shift_bits);
	}
	else if (sf == 2) {
	  jxr_set_OUTPUT_BITDEPTH(image, JXR_BD16S);
	  jxr_set_SHIFT_BITS(image, shift_bits);
	}
	else if (sf == 3)
	  jxr_set_OUTPUT_BITDEPTH(image, JXR_BD16F);    
	break;
      case 32:
	/* THOR fix: shift_bits should default to 10 if no shift bits are present */
	if (shift_bits == 0)
	  shift_bits = 10;
	if (sf == 2) {
	  jxr_set_OUTPUT_BITDEPTH(image, JXR_BD32S);
	  jxr_set_SHIFT_BITS(image, shift_bits);
	} else if (sf == 3) {
	  jxr_set_OUTPUT_BITDEPTH(image, JXR_BD32F);    
	  jxr_set_FLOAT(image, len_mantissa, exp_bias);    
	}
	break;
      case 5:
	jxr_set_OUTPUT_BITDEPTH(image, JXR_BD5);
	break;
      case 6:
	jxr_set_OUTPUT_BITDEPTH(image, JXR_BD565);
	break;
      case 10:
	jxr_set_OUTPUT_BITDEPTH(image, JXR_BD10);
	break;
      default:
	assert(!"unsupported number of bits per pixel");
	break;
      }
    } else { /* raw */
        if ((raw_info_t.raw_format >= 3) && (raw_info_t.raw_format <= 14)) { /* N-channel and N-channel Alpha*/
	  if(!alpha_plane)
	    jxr_set_OUTPUT_CLR_FMT(image, JXR_OCF_NCOMPONENT);
	  else
	    jxr_set_OUTPUT_CLR_FMT(image, JXR_OCF_YONLY);
	  if (raw_info_t.raw_format <= 8) {
	    jxr_set_INTERNAL_CLR_FMT(image, JXR_NCOMPONENT, ncomp, 0); /* N-channel */
	  } else {
	    if(!alpha_plane)
	      jxr_set_INTERNAL_CLR_FMT(image, JXR_NCOMPONENT, ncomp - 1, 1); /* N-channel Alpha */
	    else
	      jxr_set_INTERNAL_CLR_FMT(image, JXR_YONLY, 1, 0); /* N-channel Alpha */                
	  }
	  if (8 == raw_info_t.raw_bpc) 
	    jxr_set_OUTPUT_BITDEPTH(image, JXR_BD8);    
	  else 
	    jxr_set_OUTPUT_BITDEPTH(image, JXR_BD16);    
        } else if (raw_info_t.raw_format == 15) {
	  jxr_set_INTERNAL_CLR_FMT(image, JXR_YUV444, 3,0);
	  jxr_set_OUTPUT_CLR_FMT(image, JXR_OCF_RGBE);
	  jxr_set_OUTPUT_BITDEPTH(image, JXR_BD8);    
        } else if (raw_info_t.raw_format == 16) {
	  jxr_set_INTERNAL_CLR_FMT(image, internal_color_fmt, 3, 0);
	  jxr_set_OUTPUT_CLR_FMT(image, JXR_OCF_RGB);
	  jxr_set_OUTPUT_BITDEPTH(image, JXR_BD5);    
        } else if (raw_info_t.raw_format == 17) {
	  jxr_set_INTERNAL_CLR_FMT(image, internal_color_fmt, 3, 0);
	  jxr_set_OUTPUT_CLR_FMT(image, JXR_OCF_RGB);
	  jxr_set_OUTPUT_BITDEPTH(image, JXR_BD565);    
        } else if (raw_info_t.raw_format == 18) {
	  jxr_set_INTERNAL_CLR_FMT(image, internal_color_fmt, 3, 0);
	  jxr_set_OUTPUT_CLR_FMT(image, JXR_OCF_RGB);
	  jxr_set_OUTPUT_BITDEPTH(image, JXR_BD10);    
        } else if (raw_info_t.raw_format == 19 || raw_info_t.raw_format == 23) {
	  if(!alpha_plane) {
	    jxr_set_INTERNAL_CLR_FMT(image, JXR_YUV420, 3, raw_info_t.raw_format == 23);
	    jxr_set_OUTPUT_CLR_FMT(image, JXR_OCF_YUV420);
	  } else {
	    jxr_set_INTERNAL_CLR_FMT(image, JXR_YONLY, 1, 0);
	    jxr_set_OUTPUT_CLR_FMT(image, JXR_OCF_YONLY);
	  }
	  jxr_set_OUTPUT_BITDEPTH(image, JXR_BD8);
        } else if (raw_info_t.raw_format == 20 || raw_info_t.raw_format == 24) {
	  if(!alpha_plane) {
	    jxr_set_INTERNAL_CLR_FMT(image, JXR_YUV422, 3, raw_info_t.raw_format == 24);
	    jxr_set_OUTPUT_CLR_FMT(image, JXR_OCF_YUV422);
	  } else {
	    jxr_set_INTERNAL_CLR_FMT(image, JXR_YONLY, 1, 0);
	    jxr_set_OUTPUT_CLR_FMT(image, JXR_OCF_YONLY);
	  }
	  if (8 == raw_info_t.raw_bpc)
	    jxr_set_OUTPUT_BITDEPTH(image, JXR_BD8);
	  else if (10 == raw_info_t.raw_bpc)
	    jxr_set_OUTPUT_BITDEPTH(image, JXR_BD10);
	  else
	    jxr_set_OUTPUT_BITDEPTH(image, JXR_BD16);
        } else if (raw_info_t.raw_format == 21 || raw_info_t.raw_format == 25) {
	  if(!alpha_plane) {
	    jxr_set_INTERNAL_CLR_FMT(image, internal_color_fmt, 3, raw_info_t.raw_format == 25);
	    jxr_set_OUTPUT_CLR_FMT(image, JXR_OCF_YUV444);
	  } else {
	    jxr_set_INTERNAL_CLR_FMT(image, JXR_YONLY, 1, 0);
	    jxr_set_OUTPUT_CLR_FMT(image, JXR_OCF_YONLY);
	  }
	  if (8 == raw_info_t.raw_bpc)
	    jxr_set_OUTPUT_BITDEPTH(image, JXR_BD8);
	  else if (10 == raw_info_t.raw_bpc)
	    jxr_set_OUTPUT_BITDEPTH(image, JXR_BD10);
	  else
	    jxr_set_OUTPUT_BITDEPTH(image, JXR_BD16);
        } else if (raw_info_t.raw_format == 22 || raw_info_t.raw_format == 26) {
	  if(!alpha_plane) {
	    jxr_set_INTERNAL_CLR_FMT(image, internal_color_fmt, 3,raw_info_t.raw_format == 26);
	    jxr_set_OUTPUT_CLR_FMT(image, JXR_OCF_YUV444);
	  } else {
	    jxr_set_INTERNAL_CLR_FMT(image, JXR_YONLY, 1, 0);
	    jxr_set_OUTPUT_CLR_FMT(image, JXR_OCF_YONLY);
	  }
	  jxr_set_OUTPUT_BITDEPTH(image, JXR_BD16S);
        } else if (raw_info_t.raw_format == 27 || raw_info_t.raw_format == 28) {
	  if(!alpha_plane) {
	    jxr_set_INTERNAL_CLR_FMT(image, JXR_YUVK, 4,raw_info_t.raw_format == 28);
	    jxr_set_OUTPUT_CLR_FMT(image, JXR_OCF_CMYKDIRECT);
	  } else {
	    jxr_set_INTERNAL_CLR_FMT(image, JXR_YONLY, 1, 0);
	    jxr_set_OUTPUT_CLR_FMT(image, JXR_OCF_YONLY);
	  }
	  if (8 == raw_info_t.raw_bpc) {
	    jxr_set_OUTPUT_BITDEPTH(image, JXR_BD8);
	  } else {
	    jxr_set_OUTPUT_BITDEPTH(image, JXR_BD16);
	  }
        } else if (raw_info_t.raw_format == 29) {
	  jxr_set_INTERNAL_CLR_FMT(image, internal_color_fmt, 3, 0);
	  jxr_set_OUTPUT_CLR_FMT(image, JXR_OCF_RGB);
	  jxr_set_OUTPUT_BITDEPTH(image, JXR_BD8);    
        } else if (raw_info_t.raw_format == 30) {
	  jxr_set_INTERNAL_CLR_FMT(image, internal_color_fmt, 3, 1);
	  jxr_set_OUTPUT_CLR_FMT(image, JXR_OCF_RGB);
	  jxr_set_OUTPUT_BITDEPTH(image, JXR_BD8);       
        } else if (raw_info_t.raw_format == 31) {
	  if(!alpha_plane) {
	    jxr_set_INTERNAL_CLR_FMT(image, internal_color_fmt, 3, 1);
	    jxr_set_OUTPUT_CLR_FMT(image, JXR_OCF_RGB);
	    jxr_set_OUTPUT_BITDEPTH(image, JXR_BD8);
	  } else {
	    jxr_set_INTERNAL_CLR_FMT(image, JXR_YONLY, 1, 0);
	    jxr_set_OUTPUT_CLR_FMT(image, JXR_OCF_YONLY);
	    jxr_set_OUTPUT_BITDEPTH(image, JXR_BD8);             
	  }      
        } else if (raw_info_t.raw_format == 32) {
	  if(!alpha_plane) {
	    jxr_set_INTERNAL_CLR_FMT(image, internal_color_fmt, 3, 1);
	    jxr_set_OUTPUT_CLR_FMT(image, JXR_OCF_RGB);
	    jxr_set_OUTPUT_BITDEPTH(image, JXR_BD8);
	  } else {
	    jxr_set_INTERNAL_CLR_FMT(image, JXR_YONLY, 1, 0);
	    jxr_set_OUTPUT_CLR_FMT(image, JXR_OCF_YONLY);
	    jxr_set_OUTPUT_BITDEPTH(image, JXR_BD8);     
	    premultiplied = 1;
	  }
        } else if (raw_info_t.raw_format == 33) {
	  if(!alpha_plane) {
	    jxr_set_INTERNAL_CLR_FMT(image, internal_color_fmt, 3, 1);
	    jxr_set_OUTPUT_CLR_FMT(image, JXR_OCF_RGB);
	    jxr_set_OUTPUT_BITDEPTH(image, JXR_BD16);
	  } else {
	    jxr_set_INTERNAL_CLR_FMT(image, JXR_YONLY, 1, 0);
	    jxr_set_OUTPUT_CLR_FMT(image, JXR_OCF_YONLY);
	    jxr_set_OUTPUT_BITDEPTH(image, JXR_BD16);         
	    premultiplied = 1;
	  }
        } else if (raw_info_t.raw_format == 34) {
	  if(!alpha_plane) {
	    jxr_set_INTERNAL_CLR_FMT(image, internal_color_fmt, 3,1);
	    jxr_set_OUTPUT_CLR_FMT(image, JXR_OCF_RGB);
	    jxr_set_OUTPUT_BITDEPTH(image, JXR_BD32F);             
	    jxr_set_FLOAT(image, len_mantissa, exp_bias);   
	  } else {
	    jxr_set_INTERNAL_CLR_FMT(image, JXR_YONLY, 1,0);
	    jxr_set_OUTPUT_CLR_FMT(image, JXR_OCF_YONLY);
	    jxr_set_OUTPUT_BITDEPTH(image, JXR_BD32F);          
	    jxr_set_FLOAT(image, len_mantissa, exp_bias);    
	    premultiplied = 1;
	  }
        }        

        if(alpha_mode == 2 && !alpha_plane) {
	  set_ncomp(input_handle, ncomp - 1);
        } else if(alpha_mode == 2 && alpha_plane) {
	  set_ncomp(input_handle, 1);
        }
    }

    jxr_set_BANDS_PRESENT(image, bands_present);
    jxr_set_TRIM_FLEXBITS(image, trim_flexbits);
    jxr_set_OVERLAP_FILTER(image, overlap_filter);
    jxr_set_DISABLE_TILE_OVERLAP(image, disable_tile_overlap);
    jxr_set_FREQUENCY_MODE_CODESTREAM_FLAG(image, frequency_mode_flag);
    jxr_set_PROFILE_IDC(image, profile_idc);
    jxr_set_LEVEL_IDC(image, level_idc);
    jxr_set_LONG_WORD_FLAG(image, long_word_flag_setting);
    jxr_set_ALPHA_IMAGE_PLANE_FLAG(image, alpha_mode == 1 ? 1 : 0,premultiplied);

    jxr_set_NUM_VER_TILES_MINUS1(image, tile_columns);
    jxr_set_TILE_WIDTH_IN_MB(image, tile_width_in_MB);
    jxr_set_NUM_HOR_TILES_MINUS1(image, tile_rows);
    jxr_set_TILE_HEIGHT_IN_MB(image, tile_height_in_MB);
    //jxr_set_pixel_format(image, pxl_fmt);

    if (quant_uniform) {
      if (alpha_mode == 2 && alpha_plane) {
	assert(ncomp == 1);
	if (quant_per_channel_count == 1) {
	  jxr_set_QP_UNIFORM(image, quant_per_channel[0]);
	} else {
	  jxr_set_QP_UNIFORM(image, quant_per_channel[quant_per_channel_count]);
	}
      } else if (quant_per_channel_count == 1) {
	jxr_set_QP_UNIFORM(image, quant_per_channel[0]);
      } else if (ncomp >= 3 && quant_per_channel_count == 2) {
	jxr_set_QP_SEPARATE(image, quant_per_channel);
      } else {
	int idx;
	for (idx = ncomp ; idx < (int) quant_per_channel_count ; idx += 1)
	  quant_per_channel[idx] = quant_per_channel[ncomp-1];
	
	jxr_set_QP_INDEPENDENT(image, quant_per_channel);
      }
    } else if (quant_path) {
      FILE*quant_fd = fopen(quant_path, "r");
      if (quant_fd == 0) {
	perror(quant_path);
	return 1;
      }
      int rc = qp_parse_file(quant_fd, image);
      if (rc < 0) return 1;
      
    } else {
      jxr_set_QP_LOSSLESS(image);
    }
    
    rc = jxr_test_PROFILE_IDC(image, 0);
    if (rc < 0) {        
        return 1;
    }

    rc = jxr_test_LEVEL_IDC(image, 0);
    if (rc < 0) {        
        return 1;
    }
    return 0;
}


int setup_container_params(jxr_container_t container, void *input_handle)
{
    int wid, hei, ncomp, bpi;
    short sf, photometric;
    int padBytes;
    int ycc;
    int premultiplied;
    int alpha;

    get_file_parameters(input_handle, &wid, &hei, &ncomp, &bpi, &sf, &photometric, &padBytes,&ycc,&premultiplied,&alpha);
    int alpha_present = 0;
    if (!raw_info_t.is_raw) {
      int rawcomp = ncomp + padBytes - ((alpha)?(1):(0));
      if (photometric == -1 && rawcomp >= 3 && rawcomp <= 8 && (bpi == 8 || bpi == 16)) {
	/* Use the n-channel encoding in case the format is undefined */
	if (alpha)
	  alpha_present = 1;
	
	switch(rawcomp) {
	case 3:
	  if (bpi == 8) {
	    pxl_fmt = (alpha)?(JXRC_FMT_32bpp3ChannelsAlpha):(JXRC_FMT_24bpp3Channels);
	  } else {
	    pxl_fmt = (alpha)?(JXRC_FMT_64bpp3ChannelsAlpha):(JXRC_FMT_48bpp3Channels);
	  }
	  break;
	case 4:
	  if (bpi == 8) {
	    pxl_fmt = (alpha)?(JXRC_FMT_40bpp4ChannelsAlpha):(JXRC_FMT_32bpp4Channels);
	  } else {
	    pxl_fmt = (alpha)?(JXRC_FMT_80bpp4ChannelsAlpha):(JXRC_FMT_64bpp4Channels);
	  }
	  break;
	case 5:
	  if (bpi == 8) {
	    pxl_fmt = (alpha)?(JXRC_FMT_48bpp5ChannelsAlpha):(JXRC_FMT_40bpp5Channels);
	  } else {
	    pxl_fmt = (alpha)?(JXRC_FMT_96bpp5ChannelsAlpha):(JXRC_FMT_80bpp5Channels);
	  }
	  break;
	case 6:
	  if (bpi == 8) {
	    pxl_fmt = (alpha)?(JXRC_FMT_56bpp6ChannelsAlpha):(JXRC_FMT_48bpp6Channels);
	  } else {
	    pxl_fmt = (alpha)?(JXRC_FMT_112bpp6ChannelsAlpha):(JXRC_FMT_96bpp6Channels);
	  }
	  break;
	case 7:
	  if (bpi == 8) {
	    pxl_fmt = (alpha)?(JXRC_FMT_64bpp7ChannelsAlpha):(JXRC_FMT_56bpp7Channels);
	  } else {
	    pxl_fmt = (alpha)?(JXRC_FMT_128bpp7ChannelsAlpha):(JXRC_FMT_112bpp7Channels);
	  }
	  break;
	case 8:
	  if (bpi == 8) {
	    pxl_fmt = (alpha)?(JXRC_FMT_72bpp8ChannelsAlpha):(JXRC_FMT_64bpp8Channels);
	  } else {
	    pxl_fmt = (alpha)?(JXRC_FMT_144bpp8ChannelsAlpha):(JXRC_FMT_128bpp8Channels);
	  }
	  break;
	}
      } else {
	switch (ncomp + padBytes) {
	case 1:
	  if (bpi == 1) {
	    pxl_fmt = JXRC_FMT_BlackWhite;
	  } else if (bpi == 8) {
	    pxl_fmt = JXRC_FMT_8bppGray;
	  } else if (bpi == 16) {
	    if(sf == 1)
	      pxl_fmt = JXRC_FMT_16bppGray;
	    else if(sf == 2)
	      pxl_fmt = JXRC_FMT_16bppGrayFixedPoint;
	    else if(sf == 3)
	      pxl_fmt = JXRC_FMT_16bppGrayHalf;                
	  } else if (bpi == 32) {
	    if(sf == 2)
	      pxl_fmt = JXRC_FMT_32bppGrayFixedPoint;                
	    else if(sf == 3)
	      pxl_fmt = JXRC_FMT_32bppGrayFloat;
	  }
	  break;
	case 3: /* Assume RGB */
	  if (bpi == 8) {
	    if (photometric == 2) 
	      pxl_fmt = JXRC_FMT_24bppRGB;
	    else if (photometric == 6) {
	      switch(ycc) {
	      case 1:
		pxl_fmt = JXRC_FMT_12bppYCC420;
		break;
	      case 2:
		pxl_fmt = JXRC_FMT_16bppYCC422;
		break;
	      case 3:
		pxl_fmt = JXRC_FMT_24bppYCC444;
		break;
	      }
	    }
	  } else if (bpi == 16) {
	    if(sf == 1) {
	      if (photometric == 2) 
		pxl_fmt = JXRC_FMT_48bppRGB;
	      else if (photometric == 6) {
		switch(ycc) {
		case 2:
		  pxl_fmt = JXRC_FMT_32bppYCC422;
		  break;
		case 3:
		  pxl_fmt = JXRC_FMT_48bppYCC444;
		  break;
		}
	      }
	    } else if(sf == 2) {
	      if (photometric == 2)
		pxl_fmt = JXRC_FMT_48bppRGBFixedPoint;
	      else if (photometric == 6 && ycc == 3)
		pxl_fmt = JXRC_FMT_48bppYCC444FixedPoint;
	    } else if(sf == 3)
	      pxl_fmt = JXRC_FMT_48bppRGBHalf;
	  } else if (bpi == 32) {
	    if(sf == 2)
	      pxl_fmt = JXRC_FMT_96bppRGBFixedPoint;
	    else if (sf == 3)
	      pxl_fmt = JXRC_FMT_128bppRGBFloat; /* no 96bppRGBFloat, encode as 128bppRGBFloat */
	  } else if (bpi == 5) {
	    pxl_fmt = JXRC_FMT_16bppBGR555;
	  } else if (bpi == 6) {
	    pxl_fmt = JXRC_FMT_16bppBGR565;
	  } else if (bpi == 10) {
	    if (photometric == 2)
	      pxl_fmt = JXRC_FMT_32bppBGR101010;
	    else if (photometric == 6) {
	      switch(ycc) {
	      case 2:
		pxl_fmt = JXRC_FMT_20bppYCC422;
		break;
	      case 3:
		pxl_fmt = JXRC_FMT_30bppYCC444;
		break;
	      }
	    }
	  }
	  break;
	case 4: /* CMYK or RGBA or RGB_Null*/
	  if (bpi == 8) {
	    if (photometric == 5) {
	      pxl_fmt = JXRC_FMT_32bppCMYK;
	      /* The raw format is 3 in case it has never been defined before, i.e. the user
		 gave no further instructions
	      */
	      if (alternate_format || raw_info_t.raw_format == 27 || raw_info_t.raw_format == 28) {
		pxl_fmt = JXRC_FMT_32bppCMYKDIRECT;
	      }
	    } else if (photometric == 2) {
	      if (padded_format) {
		pxl_fmt = JXRC_FMT_32bppBGR;
	      } else {
		pxl_fmt = JXRC_FMT_32bppBGRA;
		if (raw_info_t.raw_format == 32 || premultiplied)
		  pxl_fmt = JXRC_FMT_32bppPBGRA;
		alpha_present = 1;
	      }
	    } else if (photometric == 6) {
	      alpha_present = 1;
	      switch(ycc) {
	      case 1:
		pxl_fmt = JXRC_FMT_20bppYCC420Alpha;
		break;
	      case 2:
		pxl_fmt = JXRC_FMT_24bppYCC422Alpha;
		break;
	      case 3:
		pxl_fmt = JXRC_FMT_32bppYCC444Alpha;
		break;
	      }
	    } else if (photometric == -1) { // special
	      pxl_fmt = JXRC_FMT_32bppRGBE;
	      break;
	    }
	  } else if (bpi == 16) {
	    if (photometric == 5) {
	      pxl_fmt = JXRC_FMT_64bppCMYK;
	      if (raw_info_t.raw_format == 27 || raw_info_t.raw_format == 28) {
		pxl_fmt = JXRC_FMT_64bppCMYKDIRECT;
	      }
	    } else if (photometric == 2) {
	      if (sf == 1) {
		alpha_present = 1;
		pxl_fmt = JXRC_FMT_64bppRGBA; /* no 64bppRGB */
		if (raw_info_t.raw_format == 33)
		  pxl_fmt = JXRC_FMT_64bppPRGBA;
	      } else if (sf == 2) {
		if (ncomp == 3)
		  pxl_fmt = JXRC_FMT_64bppRGBFixedPoint;
		else if (ncomp == 4) {
		  alpha_present = 1;
		  pxl_fmt = JXRC_FMT_64bppRGBAFixedPoint;
		}
	      } else if (sf == 3) {
		if (ncomp == 3)
		  pxl_fmt = JXRC_FMT_64bppRGBHalf;                        
		else if (ncomp == 4) {
		  alpha_present = 1;
		  pxl_fmt = JXRC_FMT_64bppRGBAHalf;
		}
	      }
	    } else if (photometric == 6) {
	      alpha_present = 1;
	      switch(ycc) {
	      case 2:
		pxl_fmt = JXRC_FMT_48bppYCC422Alpha;
		break;
	      case 3:
		if (sf == 1)
		  pxl_fmt = JXRC_FMT_64bppYCC444Alpha;
		else if (sf == 2)
		  pxl_fmt = JXRC_FMT_64bppYCC444AlphaFixedPoint;
		break;
	      }
	    }
	  } else if (bpi == 32) {
	    if (photometric == 2) {
	      if (sf == 2) {
		if (ncomp == 3)
		  pxl_fmt = JXRC_FMT_128bppRGBFixedPoint;
		if (ncomp == 4) {
		  alpha_present = 1;
		  pxl_fmt = JXRC_FMT_128bppRGBAFixedPoint;
		}
	      } else if(sf == 3) {
		if (ncomp == 3)
		  pxl_fmt = JXRC_FMT_128bppRGBFloat;
		else if (ncomp == 4) {
		  alpha_present = 1;
		  pxl_fmt = JXRC_FMT_128bppRGBAFloat;
		  if (raw_info_t.raw_format == 34)
		    pxl_fmt = JXRC_FMT_128bppPRGBAFloat;
		  
		}
	      }
	    }
	  } else if (bpi == 10) {
	    if (photometric == 6) {
	      alpha_present = 1;
	      switch(ycc) {
	      case 2:
		pxl_fmt = JXRC_FMT_30bppYCC422Alpha;
		break;
	      case 3:
		pxl_fmt = JXRC_FMT_40bppYCC444Alpha;
		break;
	      }
	    }
	  }
	  break;
	case 5: /* CMYKA */
	  if (bpi == 8) {
	    if (1 || photometric == 5) { /* make this the default, even if photometry is absent */
	      alpha_present = 1;
	      pxl_fmt = JXRC_FMT_40bppCMYKAlpha;
	      if (raw_info_t.raw_format == 27 || raw_info_t.raw_format == 28) {
		pxl_fmt = JXRC_FMT_40bppCMYKDIRECTAlpha;
	      }
	    }
	  } else if (bpi == 16) {
	    if (1 || photometric == 5) { /* make this the default, even if photometry is absent */
	      alpha_present = 1;
	      pxl_fmt = JXRC_FMT_80bppCMYKAlpha;
	      if (raw_info_t.raw_format == 27 || raw_info_t.raw_format == 28) {
		pxl_fmt = JXRC_FMT_80bppCMYKDIRECTAlpha;
	      }
	    }
	  }
	  break;
	case 6:
	  if (bpi == 8) {
	    pxl_fmt = JXRC_FMT_48bpp6Channels;
	  } else if (bpi == 16) {
	    pxl_fmt = JXRC_FMT_96bpp6Channels;
	  }
	  break;
	case 7:
	  if (bpi == 8) {
	    pxl_fmt = JXRC_FMT_56bpp7Channels;
	  } else {
	    pxl_fmt = JXRC_FMT_112bpp7Channels;
	  }
	  break;
	case 8:
	  if (bpi == 8) {
	    pxl_fmt = JXRC_FMT_64bpp8Channels;
	  } else {
	    pxl_fmt = JXRC_FMT_128bpp8Channels;
	  }
	  break;
	case 9:
	  if (bpi == 8) {
	    alpha_present = 1;
	    pxl_fmt = JXRC_FMT_72bpp8ChannelsAlpha;
	  } else {
	    alpha_present = 1;
	    pxl_fmt = JXRC_FMT_144bpp8ChannelsAlpha;
	  }
	  break;
	default:
	  assert(!"unsupported number of channels");
	  break;
	}
      }
    } else { /* raw */
        if ((raw_info_t.raw_format >= 3) && (raw_info_t.raw_format <= 14)) { /* N-channel */
            if (8 == raw_info_t.raw_bpc) { 
                switch (raw_info_t.raw_format) {
                    case 3: 
                        pxl_fmt = JXRC_FMT_24bpp3Channels;
                        break;
                    case 4: 
                        pxl_fmt = JXRC_FMT_32bpp4Channels;
                        break;
                    case 5: 
                        pxl_fmt = JXRC_FMT_40bpp5Channels;
                        break;
                    case 6: 
                        pxl_fmt = JXRC_FMT_48bpp6Channels;
                        break;
                    case 7: 
                        pxl_fmt = JXRC_FMT_56bpp7Channels;
                        break;
                    case 8: 
                        pxl_fmt = JXRC_FMT_64bpp8Channels;
                        break;
                    case 9: 
                        alpha_present = 1;
                        pxl_fmt = JXRC_FMT_32bpp3ChannelsAlpha;
                        break;
                    case 10: 
                        alpha_present = 1;
                        pxl_fmt = JXRC_FMT_40bpp4ChannelsAlpha;
                        break;
                    case 11: 
                        alpha_present = 1;
                        pxl_fmt = JXRC_FMT_48bpp5ChannelsAlpha;
                        break;
                    case 12: 
                        alpha_present = 1;
                        pxl_fmt = JXRC_FMT_56bpp6ChannelsAlpha;
                        break;
                    case 13: 
                        alpha_present = 1;
                        pxl_fmt = JXRC_FMT_64bpp7ChannelsAlpha;
                        break;
                    case 14: 
                        alpha_present = 1;
                        pxl_fmt = JXRC_FMT_72bpp8ChannelsAlpha;
                        break;
                    default:
                        assert(!"unsupported raw format");
                        break;

                }
            }
            else { /* 16bpc */
                switch (raw_info_t.raw_format) {
                    case 3: 
                        pxl_fmt = JXRC_FMT_48bpp3Channels;
                        break;
                    case 4: 
                        pxl_fmt = JXRC_FMT_64bpp4Channels;
                        break;
                    case 5: 
                        pxl_fmt = JXRC_FMT_80bpp5Channels;
                        break;
                    case 6: 
                        pxl_fmt = JXRC_FMT_96bpp6Channels;
                        break;
                    case 7: 
                        pxl_fmt = JXRC_FMT_112bpp7Channels;
                        break;
                    case 8: 
                        pxl_fmt = JXRC_FMT_128bpp8Channels;
                        break;
                    case 9: 
                        alpha_present = 1;
                        pxl_fmt = JXRC_FMT_64bpp3ChannelsAlpha;
                        break;
                    case 10: 
                        alpha_present = 1;
                        pxl_fmt = JXRC_FMT_80bpp4ChannelsAlpha;
                        break;
                    case 11: 
                        alpha_present = 1;
                        pxl_fmt = JXRC_FMT_96bpp5ChannelsAlpha;
                        break;
                    case 12: 
                        alpha_present = 1;
                        pxl_fmt = JXRC_FMT_112bpp6ChannelsAlpha;
                        break;
                    case 13: 
                        alpha_present = 1;
                        pxl_fmt = JXRC_FMT_128bpp7ChannelsAlpha;
                        break;
                    case 14: 
                        alpha_present = 1;
                        pxl_fmt = JXRC_FMT_144bpp8ChannelsAlpha;
                        break;
                    default:
                        assert(!"unsupported raw format");
                        break;
                }
            }
        }
        else if (raw_info_t.raw_format == 15) { /* RGBE */
            pxl_fmt = JXRC_FMT_32bppRGBE;        
        }
        else if (raw_info_t.raw_format == 16) { 
            pxl_fmt = JXRC_FMT_16bppBGR555;        
        }
        else if (raw_info_t.raw_format == 17) { 
            pxl_fmt = JXRC_FMT_16bppBGR565;        
        }
        else if (raw_info_t.raw_format == 18) { 
            pxl_fmt = JXRC_FMT_32bppBGR101010;        
        }
        else if ((raw_info_t.raw_format >= 19) && (raw_info_t.raw_format <= 26)) { /* YCC */
            if (8 == raw_info_t.raw_bpc) { 
                switch (raw_info_t.raw_format) {
                    case 19:
                        pxl_fmt = JXRC_FMT_12bppYCC420;
                        break;
                    case 20:
                        pxl_fmt = JXRC_FMT_16bppYCC422;
                        break;
                    case 21:
                        pxl_fmt = JXRC_FMT_24bppYCC444;
                        break;
                    case 23:
                        alpha_present = 1;
                        pxl_fmt = JXRC_FMT_20bppYCC420Alpha;
                        break;
                    case 24:
                        alpha_present = 1;
                        pxl_fmt = JXRC_FMT_24bppYCC422Alpha;
                        break;
                    case 25:
                        alpha_present = 1;
                        pxl_fmt = JXRC_FMT_32bppYCC444Alpha;
                        break;
                    default:
                        assert(!"unsupported raw format");
                        break;
                }
            }
            else if (10 == raw_info_t.raw_bpc) { 
                switch (raw_info_t.raw_format) {
                    case 20: 
                        pxl_fmt = JXRC_FMT_20bppYCC422;
                        break;
                    case 21: 
                        pxl_fmt = JXRC_FMT_30bppYCC444;
                        break;
                    case 24:
                        alpha_present = 1;
                        pxl_fmt = JXRC_FMT_30bppYCC422Alpha;
                        break;
                    case 25:
                        alpha_present = 1;
                        pxl_fmt = JXRC_FMT_40bppYCC444Alpha;
                        break;
                    default:
                        assert(!"unsupported raw format");
                        break;
                }
            }
            else if (16 == raw_info_t.raw_bpc) {  /* 16bpc */
                switch (raw_info_t.raw_format) {
                    case 20:
                        pxl_fmt = JXRC_FMT_32bppYCC422;
                        break;
                    case 21:
                        pxl_fmt = JXRC_FMT_48bppYCC444;
                        break;
                    case 22:
                        pxl_fmt = JXRC_FMT_48bppYCC444FixedPoint;
                        break;
                    case 24:
                        alpha_present = 1;
                        pxl_fmt = JXRC_FMT_48bppYCC422Alpha;
                        break;
                    case 25:
                        alpha_present = 1;
                        pxl_fmt = JXRC_FMT_64bppYCC444Alpha;
                        break;
                    case 26:
                        alpha_present = 1;
                        pxl_fmt = JXRC_FMT_64bppYCC444AlphaFixedPoint;
                        break;
                    default:
                        assert(!"unsupported raw format");
                        break;
                }
            }
            else
                assert(!"unsupported raw format");
        }
        else if ((raw_info_t.raw_format >= 27) && (raw_info_t.raw_format <= 28)) { /* CMYKDIRECT */
            if (8 == raw_info_t.raw_bpc) { 
                switch (raw_info_t.raw_format) {
                    case 27:
                        pxl_fmt = JXRC_FMT_32bppCMYKDIRECT;
                        break;
                    case 28:
                        alpha_present = 1;
                        pxl_fmt = JXRC_FMT_40bppCMYKDIRECTAlpha;
                        break;
                    default:
                        assert(!"unsupported raw format");
                        break;
                }
            }
            else if (16 == raw_info_t.raw_bpc) {  /* 16bpc */
                switch (raw_info_t.raw_format) {
                    case 27:
                        pxl_fmt = JXRC_FMT_64bppCMYKDIRECT;
                        break;
                    case 28:
                        alpha_present = 1;
                        pxl_fmt = JXRC_FMT_80bppCMYKDIRECTAlpha;
                        break;
                    default:
                        assert(!"unsupported raw format");
                        break;
                }
            }
            else
                assert(!"unsupported raw format");
        }
        else if (raw_info_t.raw_format == 29) { 
            pxl_fmt = JXRC_FMT_24bppBGR;
        }
        else if (raw_info_t.raw_format == 30) {             
            pxl_fmt = JXRC_FMT_32bppBGR;
        }
        else if (raw_info_t.raw_format == 31) { 
            alpha_present = 1;
            pxl_fmt = JXRC_FMT_32bppBGRA;
        }
        else if (raw_info_t.raw_format == 32) { 
            alpha_present = 1;
            pxl_fmt = JXRC_FMT_32bppPBGRA;
        }
        else if (raw_info_t.raw_format == 33) { 
            alpha_present = 1;
            pxl_fmt = JXRC_FMT_64bppPRGBA;
        }
        else if (raw_info_t.raw_format == 34) { 
            alpha_present = 1;
            pxl_fmt = JXRC_FMT_128bppPRGBAFloat;
        }
        else
            assert(!"unsupported raw format");
    }

    jxrc_set_pixel_format(container, pxl_fmt);

    if (alpha_present) {
      if(alpha_mode == 0) {
	/* No -a option was used at command line, so by default, turn on Separate alpha */
	alpha_mode = 2; /* Separate alpha */
	//fprintf(stderr, " Setting alpha_mode to 2\n");
      }
    } else {
      /* Current output format has no alpha information, so turn off alpha_mode */
      if(alpha_mode != 0) {
	alpha_mode = 0;
	//fprintf(stderr, " Setting alpha_mode to 0\n");
      }
    }

    jxrc_set_image_shape(container, 
			 wid - window_params[2] - window_params[4], 
			 hei - window_params[1] - window_params[3]);
    jxrc_set_image_band_presence(container, (unsigned) bands_present); 
    /* another call will need to be added for separate alpha */
    
    jxrc_set_image_profile(container,profile_idc);

    if (bpi == 1) {
      if (photometric) /*white is 1 */
	jxrc_set_inverted_bw(container,0);
      else
	jxrc_set_inverted_bw(container,1);
    }

    return 0;
}

static int main_compress(const char*path)
{
    int rc = 0;
    jxr_image_t image = NULL;
    void *input_handle = NULL;
    jxr_container_t container = NULL;
    unsigned char temp_window_params[5] = {0,0,0,0,0};

    if (path_out == 0)
        path_out = "out.jxr";

    FILE*fd = fopen(path_out, "wb");
    if (fd == 0) {
        perror(path_out);
        rc = -1;
        goto exit;
    }

    /* Create the file container and bind the FD to it. */
    container = jxr_create_container();
    input_handle = open_input_file(path, container, &raw_info_t, &alpha_mode, &padded_format);
    char path_primary[2048];
    char path_alpha[2048];
    int wid, hei, ncomp, bpi;
    short sf, photometric;
    int padBytes;
    get_file_parameters(input_handle, &wid, &hei, &ncomp, &bpi, &sf, &photometric, &padBytes,NULL,NULL,NULL);

    jxrc_set_image_profile(container,profile_idc);
    if (!box_wrapper) {
      jxrc_start_file(container, fd);
    }

    rc = setup_container_params(container, input_handle);
    if(rc != 0)
        goto exit;
    
    /* Start the next ifd entry and load it with information about
    the image to be processed. */
    if (!box_wrapper) {
      rc = jxrc_begin_ifd_entry(container);
    }
    if(rc != 0)
        goto exit;

    /* Create a stub image. */
    if(alpha_mode == 2) {
      memcpy(temp_window_params,window_params,sizeof(window_params));
      rc = setup_image_params(&image, input_handle, 0, 0); 
      /* Set up parameters for a regular image, fix later after separating primary and alpha */
    } else {
      rc = setup_image_params(&image, input_handle, alpha_mode, 0);
    }

    if(rc != 0)
        goto exit;

    if(alpha_mode == 2) {
      /* Open handle to dump  primary in tif format */
      separate_primary_alpha(image, input_handle, path_out, path_primary, path_alpha, container);
      //jxrc_image_pixelformat(container, 0));        
      close_file(input_handle);
      input_handle = NULL;
      jxr_set_TILE_WIDTH_IN_MB(image,NULL);
      jxr_set_TILE_HEIGHT_IN_MB(image,NULL);
      jxr_destroy(image);
      image = NULL;
      input_handle = open_input_file(path_primary, container, &raw_info_t, &alpha_mode, &padded_format);        
      get_file_parameters(input_handle, &wid, &hei, &ncomp, &bpi, &sf, &photometric, &padBytes,NULL,NULL,NULL);
      memcpy(window_params,temp_window_params,sizeof(window_params));
      rc = setup_image_params(&image, input_handle, alpha_mode, 0);        
      if(rc != 0)
	goto exit;        
      jxrc_set_separate_alpha_image_plane(container, 1);        
    } else {
      jxrc_set_separate_alpha_image_plane(container, 0);
    }

    //jxr_set_pixel_format(image, jxrc_get_pixel_format(container));
    jxr_set_R_B_swapped(image, bgr_flag);
    
    /* Close out the current IFD and start with the actual image
    data to be written. */
    if (box_wrapper) {
      rc = jxrc_start_file_boxed(container, fd);
      if (rc != 0)
	goto exit;
      jxrc_begin_image_data_boxed(container);
    } else {
      jxrc_begin_image_data(container);
    }

    jxr_set_block_input(image, read_file);  
    jxr_set_user_data(image, input_handle);

    /* Write image to the destination file. */
    rc = jxr_write_image_bitstream(image, fd);
    if (rc != 0) 
        goto exit;        

    /* Finalize IFDs for image. */
    if (box_wrapper) {
      jxrc_write_container_post_boxed(container);
    } else {
      jxrc_write_container_post(container);
    }

    close_file(input_handle);
    input_handle = NULL;
    jxr_destroy(image); 
    image = NULL;

    if(alpha_mode == 2) {
      input_handle = open_input_file(path_alpha, container, &raw_info_t, &alpha_mode, &padded_format);
      get_file_parameters(input_handle, &wid, &hei, &ncomp, &bpi, &sf, &photometric, &padBytes,NULL,NULL,NULL);
      memcpy(window_params,temp_window_params,sizeof(window_params));   
      rc = setup_image_params(&image, input_handle, alpha_mode, 1);        
      //jxr_set_pixel_format(image, jxrc_get_pixel_format(container));
      if(rc != 0)
	goto exit;
            
      jxr_set_block_input(image, read_file);  
      jxr_set_user_data(image, input_handle);
      
      /* Write image to the destination file. */
      rc = jxr_write_image_bitstream(image, fd);
      if (rc != 0)
            goto exit;
      /* Finalize IFDs for alpha plane. */
      if (box_wrapper) {
	jxrc_write_container_post_alpha_boxed(container);
      } else {
	jxrc_write_container_post_alpha(container);
      }
      remove(path_alpha);
      remove(path_primary);     
    }

    
    
exit:
    close_file(input_handle);
    jxr_destroy(image);
    if(fd)
        fclose(fd);
    jxr_destroy_container(container);
    
    if (rc != 0)  /* JNB20 inserted example */
      fprintf(stderr,"Unable to create the output file - are the settings correct and supported ?\n");

    return rc;
}

static int decompress_image(FILE *fd, jxr_container_t container, void *output_handle, jxr_image_t *pImage, 
			    unsigned char alpha)
{
    int rc, idx;
    *pImage = jxr_create_input();
    jxr_set_block_output(*pImage, write_file);
    //jxr_set_pixel_format(*pImage, jxrc_image_pixelformat(container, 0));
    jxr_set_user_data(*pImage, output_handle);
    jxr_set_PROFILE_IDC(*pImage, profile_idc);
    jxr_set_LEVEL_IDC(*pImage, level_idc);

    for (idx = 0 ; idx < nflags ; idx += 1) {
      if (strcmp(flags[idx],"SKIP_HP_DATA") == 0) {
	jxr_flag_SKIP_HP_DATA(*pImage, 1);
	continue;
      }
      if (strcmp(flags[idx],"SKIP_FLEX_DATA") == 0) {
	jxr_flag_SKIP_FLEX_DATA(*pImage, 1);
	continue;
      }
    }

    /*
    ** Start mod thor: This is the line-based decoding
    */
    if (line_mode) {
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
      rc = jxr_test_LONG_WORD_FLAG(*pImage, long_word_flag_setting);
      if (rc < 0)
	fprintf(stderr, "LONG_WORD_FLAG condition was set incorrectly\n");
    }
    return rc;

}
#define SAFE_FREE(h) {if(h)free(h); h = NULL;}
#define SAFE_CLOSE(h) {if(h)close_file(h); h = NULL;}
#define SAFE_JXR_DESTROY(h) {if(h) jxr_destroy(h); h = NULL;}

static int main_decompress(const char*path_in)
{
    int rc;
    unsigned int alphaCodedImagePresent = 0;
    void *output_handle_primary = NULL;
    void *output_handle_alpha = NULL;
    char path_out_primary[2048];
    char path_out_alpha[2048];
    unsigned long off;
    jxr_image_t imageAlpha=NULL, image=NULL;
    if (path_out == 0)
        path_out = "out.raw";

    FILE*fd = fopen(path_in, "rb");
    if (fd == 0) {
        perror(path_in);
        return -1;
    }

    jxr_container_t ifile = jxr_create_container();
    rc = jxr_read_image_container(ifile, fd);
    if (rc >= 0) {
        assert(rc >= 0);        
# if defined(DETAILED_DEBUG)
        printf("Detected jxr image container\n");
        printf("XXXX Bytes of contained image: %ld\n", jxrc_image_bytecount(ifile, 0));
#endif
        off = jxrc_image_offset(ifile, 0);
#if defined(DETAILED_DEBUG)
        printf("XXXX Offset of contained image: %ld\n", off);
#endif
        rc = fseek(fd, off, SEEK_SET);
        assert(rc >= 0);
        if(jxrc_alpha_offset(ifile, 0))
        {
            alphaCodedImagePresent = 1;
# if defined(DETAILED_DEBUG)        
            printf("XXXX Bytes of alpha image: %ld\n", jxrc_alpha_bytecount(ifile, 0));
            printf("XXXX Offset of contained image: %ld\n", jxrc_alpha_offset(ifile, 0));
#endif
        }        
    } else {        
#if defined(DETAILED_DEBUG)
        printf("No container found, assuming unwrapped bistream with no alpha coded image\n");
#endif
	// codestream parsing is broken, do not attempt
	fprintf(stderr,"input image is not a jpegxr file\n");
	return -1;
	//
        rc = fseek(fd, 0, SEEK_SET);
        assert(rc >= 0);
    }

    /* read optional IFD tags to make certain of conformance */
    char * document_name = 0, * image_description = 0, * equipment_make = 0, * equipment_model = 0, * page_name = 0;
    char * software_name_version = 0, * date_time = 0, * artist_name = 0, * host_computer = 0, * copyright_notice = 0;
    unsigned char profile, level;
    unsigned short page_number[2] = {0, 0}, color_space;
    unsigned long spatial_xfrm, image_type;
    float width_res, height_res;
    unsigned char image_band_present, alpha_band_present, buf[4];
    const char *ext = strrchr(path_out,'.');

    rc = jxrc_document_name(ifile, 0, &document_name);
    rc = jxrc_image_description(ifile, 0, &image_description);
    rc = jxrc_equipment_make(ifile, 0, &equipment_make);
    rc = jxrc_equipment_model(ifile, 0, &equipment_model);
    rc = jxrc_page_name(ifile, 0, &page_name);
    rc = jxrc_page_number(ifile, 0, page_number);
    rc = jxrc_software_name_version(ifile, 0, &software_name_version);
    rc = jxrc_date_time(ifile, 0, &date_time);
    rc = jxrc_artist_name(ifile, 0, &artist_name);
    rc = jxrc_host_computer(ifile, 0, &host_computer);
    rc = jxrc_copyright_notice(ifile, 0, &copyright_notice);
    color_space = jxrc_color_space(ifile, 0);
    spatial_xfrm = jxrc_spatial_xfrm_primary(ifile, 0);
    image_type = jxrc_image_type(ifile, 0);
    rc = jxrc_ptm_color_info(ifile, 0, buf);
    rc = jxrc_profile_level_container(ifile, 0, &profile, &level);
    if (rc < 0) {
        profile = 111;
        level = 255;
    }
    width_res = jxrc_width_resolution(ifile, 0);
    height_res = jxrc_height_resolution(ifile, 0);
    image_band_present = jxrc_image_band_presence(ifile, 0);
    alpha_band_present = jxrc_alpha_band_presence(ifile, 0);
    rc = jxrc_padding_data(ifile, 0);

    if (!strcasecmp(ext,".raw"))
      padded_format = 1;
    
    if (alphaCodedImagePresent) {
      if (!strcasecmp(ext,".raw")) {
	/* Open handle to dump decoded primary in raw format */
	strcpy(path_out_primary, path_out);
	strcat(path_out_primary, "_primary.raw");
	output_handle_primary = open_output_file(path_out_primary,ifile,padded_format,0);
      } else {
	/* Open handle to dump decoded primary in raw format, but with proper color order */
	strcpy(path_out_primary, path_out);
	strcat(path_out_primary, "_primary.craw");
	output_handle_primary = open_output_file(path_out_primary,ifile,padded_format,0);
      }
    } else {
      output_handle_primary = open_output_file(path_out,ifile,padded_format,0);
    }
    /*Decode image */
    rc = decompress_image(fd, ifile, output_handle_primary, &image, 0); 
    SAFE_CLOSE(output_handle_primary);
    if(rc < 0)
        goto exit;    

    if(!alphaCodedImagePresent)
        goto exit;   

    /* Open handle to dump decoded alpha in raw format*/
    if (!strcasecmp(ext,".raw")) {
      strcpy(path_out_alpha, path_out);
      strcat(path_out_alpha, "_alpha.raw");
    } else {
      strcpy(path_out_alpha, path_out);
      strcat(path_out_alpha, "_alpha.craw");
    }
    output_handle_alpha = open_output_file(path_out_alpha,ifile,padded_format,1);

    /*Seek to alpha offset */
    off = jxrc_alpha_offset(ifile, 0);
    rc = fseek(fd, off, SEEK_SET);
    assert(rc >= 0);
    /* Decode alpha */
    rc = decompress_image(fd, ifile, output_handle_alpha, &imageAlpha, 1); 
    SAFE_CLOSE(output_handle_alpha);
    if(rc < 0)
        goto exit;

    /* For YCC and CMYKDirect formats, concatenate alpha and primary images */
    /* For other output pixel formats, interleave alphad and primary images */
    {
      void * output_handle = open_output_file(path_out,ifile,padded_format,0);
      FILE *fpPrimary = fopen(path_out_primary,"rb");
      assert(fpPrimary);
      FILE *fpAlpha = fopen(path_out_alpha,"rb");
      assert(fpAlpha);
      jxr_set_user_data(image, output_handle);        
      write_file_combine_primary_alpha(image, fpPrimary, fpAlpha);
      fclose(fpPrimary);
      fclose(fpAlpha);
      remove(path_out_primary);
      remove(path_out_alpha);        
      SAFE_CLOSE(output_handle);
    }
    
exit:    
    SAFE_FREE(document_name);
    SAFE_FREE(image_description);
    SAFE_FREE(equipment_make);
    SAFE_FREE(equipment_model);
    SAFE_FREE(page_name);
    SAFE_FREE(software_name_version);
    SAFE_FREE(date_time);
    SAFE_FREE(artist_name);
    SAFE_FREE(host_computer);
    SAFE_FREE(copyright_notice);
    SAFE_JXR_DESTROY(image);
    SAFE_JXR_DESTROY(imageAlpha);
    if (ifile)
      jxr_destroy_container(ifile);
    fclose(fd);
    return 0;
}

/*
* $Log: jpegxr.c,v $
* Revision 1.47  2014/10/23 15:52:14  thor
* Updated the spelling in the software.
*
* Revision 1.46  2012-03-19 15:48:19  thor
* Fixed YOnly and the container_nc field of the image to contain always
* the correct number of container components including the alpha channel.
*
* Revision 1.45  2012-03-18 21:47:07  thor
* Fixed double adjustments of window parameters. Fixed handling of
* N-channel coding in tiff.
*
* Revision 1.44  2012-03-18 00:09:21  thor
* Fixed handling of YCC.
*
* Revision 1.43  2012-03-17 17:32:16  thor
* Fixed decoding of separate/interleaved alpha.
*
* Revision 1.42  2012-02-16 16:36:26  thor
* Heavily reworked, but not yet tested.
*
* Revision 1.41  2012-02-13 21:11:03  thor
* Tested now for most color formats.
*
* Revision 1.40  2012-02-13 18:56:44  thor
* Fixed parsing 565 tiff files.
*
* Revision 1.39  2012-02-13 18:23:43  thor
* Fixed writer in simple cases. Not everything explored yet.
*
* Revision 1.37  2012-02-11 04:23:31  thor
* Start of the box-based writer.
*
* Revision 1.36  2011-11-24 11:44:09  thor
* Added an R-B swap flag.
*
* Revision 1.35  2011-11-22 12:30:15  thor
* The default color space for the 4 channel generic pixel format
* is now CMYK. The premultiplied alpha flag is now set correctly on
* decoding, even for images with a separate alpha plane, and it is
* honoured correctly on encoding from tif files.
*
* Revision 1.34  2011-11-15 10:11:17  thor
* Bumped to release 1.30.
*
* Revision 1.33  2011-11-11 14:24:55  thor
* Fixed the window handling.
*
* Revision 1.32  2011-11-08 20:17:29  thor
* Merged a couple of fixes from the JNB.
*
* Revision 1.31  2011-06-10 21:42:17  thor
* Removed the timing code again from the main codeline.
*
* Revision 1.29  2011-04-28 08:45:43  thor
* Fixed compiler warnings, ported to gcc 4.4, removed obsolete files.
*
* Revision 1.28  2011-03-14 18:21:35  thor
* Bumped to 1.19. Fixed one assert.
*
* Revision 1.27  2011-03-12 19:12:22  thor
* Fixed compression of 555,565 and 101010 in raw.
*
* Revision 1.26  2011-03-09 16:19:31  thor
* Updated the JXR internal color format writing for raw images.
*
* Revision 1.25  2011-03-08 17:30:57  thor
* Upsampling from YUV42x -> YUV444 does not work.
*
* Revision 1.24  2011-03-08 14:39:29  thor
* Fixed CMYKDirect.
*
* Revision 1.23  2011-03-08 12:31:33  thor
* Fixed YUV444+alpha interleaved and again RGBA+alpha interleaved.
*
* Revision 1.22  2011-03-08 11:12:05  thor
* Fixed partially the YOnly decoding. Still bugs in YUV444 with interleaved
* alpha.
*
* Revision 1.21  2011-03-04 12:12:12  thor
* Bumped to 1.16. Fixed RGB-YOnly handling, including the handling of
* YOnly for which a new -f flag has been added.
*
* Revision 1.20  2011-02-26 10:24:39  thor
* Fixed bugs for alpha and separate alpha.
*
* Revision 1.19  2010-10-03 13:14:42  thor
* Fixed missing preshift for BD32 images, added alpha-quantizer
* parameter. Fixed alpha plane container offset.
*
* Revision 1.18  2010-09-10 15:05:45  thor
* The -f option is now no longer active for the raw 444 subsampled formats.
*
* Revision 1.17  2010-09-02 17:13:37  thor
* Raw output now automatically generates padding.
*
* Revision 1.16  2010-08-31 15:38:12  thor
* Fixed reading of CMYKDirect files, trivial component transposition is
* not correct.
*
* Revision 1.15  2010-07-12 16:06:58  thor
* Fixed BG swap for raw encoding.
*
* Revision 1.14  2010-06-26 12:32:46  thor
* Fixed RGBE encoding, a color transformation was missing. Added rgbe
* as input format.
*
* Revision 1.13  2010-06-19 11:48:36  thor
* Fixed memory leaks.
*
* Revision 1.12  2010-06-18 20:31:50  thor
* Fixed a lot of CMYK formats, YCC formats parsing.
*
* Revision 1.11  2010-06-17 22:02:14  thor
* Fixed (partially) the YCC reader. Added type recognition for TIFF.
*
* Revision 1.10  2010-05-22 22:14:35  thor
* Fixed memory leaks in the TIFF parser.
*
* Revision 1.9  2010-05-21 12:49:30  thor
* Fixed alpha encoding for BGRA32, fixed channel order in BGR555,101010 and 565
* (a double cancelation bug), fixed channel order for BGR which is really RGB.
*
* Revision 1.8  2010-05-14 12:48:12  thor
* Added tiff readers for BGR555 and BGR101010 and BGR565
*
* Revision 1.7  2010-05-13 16:30:03  thor
* Added options to set the chroma centering. Fixed writing of BGR565.
* Made the "-p" output option nicer.
*
* Revision 1.6  2010-05-05 15:13:12  thor
* Fixed BGR handling for TIF and PNM.
*
* Revision 1.5  2010-05-01 11:16:08  thor
* Fixed the tiff tag order. Added spatial/line mode.
*
* Revision 1.4  2010-03-31 07:50:58  thor
* Replaced by the latest MS version.
*
* Revision 1.39 2009/05/29 12:00:00 microsoft
* Reference Software v1.6 updates.
*
* Revision 1.38 2009/04/13 12:00:00 microsoft
* Reference Software v1.5 updates.
*
* Revision 1.37 2008/05/25 09:40:46 thor
* Added a -d command line option to enable derived quantization.
*
* Revision 1.36 2008/05/13 13:47:11 thor
* Some experiments with a smarter selection for the quantization size,
* does not yet compile.
*
* Revision 1.35 2008-05-09 19:57:48 thor
* Reformatted for unix LF.
*
* Revision 1.34 2008-04-15 14:28:12 thor
* Start of the repository for the jpegxr reference software.
*
* Revision 1.33 2008/03/21 18:05:53 steve
* Proper CMYK formatting on input.
*
* Revision 1.32 2008/03/18 18:36:56 steve
* Support compress of CMYK images.
*
* Revision 1.31 2008/03/11 22:12:49 steve
* Encode YUV422 through DC.
*
* Revision 1.30 2008/03/06 02:05:48 steve
* Distributed quantization
*
* Revision 1.29 2008/03/05 06:58:10 gus
* *** empty log message ***
*
* Revision 1.28 2008/03/05 04:04:30 steve
* Clarify constraints on USE_DC_QP in image plane header.
*
* Revision 1.27 2008/03/05 01:27:15 steve
* QP_UNIFORM may use USE_DC_LP optionally.
*
* Revision 1.26 2008/03/04 23:01:28 steve
* Cleanup QP API in preparation for distributed QP
*
* Revision 1.25 2008/03/02 19:56:27 steve
* Infrastructure to read write BD16 files.
*
* Revision 1.24 2008/03/01 02:46:09 steve
* Add support for JXR container.
*
* Revision 1.23 2008/02/29 01:03:31 steve
* MSC doesnt have strcasecmp. Use stricmp instead.
*
* Revision 1.22 2008/02/29 00:57:59 steve
* VisualStudio files
*
* Revision 1.21 2008/02/26 23:52:44 steve
* Remove ident for MS compilers.
*
* Revision 1.20 2008/02/01 22:49:53 steve
* Handle compress of YUV444 color DCONLY
*
* Revision 1.19 2008/01/19 02:30:46 rick
* Re-implement and extend file interface.
*
* Revision 1.18 2008/01/08 01:06:20 steve
* Add first pass overlap filtering.
*
* Revision 1.17 2008/01/06 01:29:28 steve
* Add support for TRIM_FLEXBITS in compression.
*
* Revision 1.16 2008/01/04 17:07:35 steve
* API interface for setting QP values.
*
* Revision 1.15 2007/12/06 00:24:25 steve
* Zero fill strip buffer, so that right pad is zeros.
*
* Revision 1.14 2007/12/04 22:06:10 steve
* Infrastructure for encoding LP.
*
* Revision 1.13 2007/11/30 01:57:00 steve
* Handle comments in PNM files.
*
* Revision 1.12 2007/11/30 01:50:58 steve
* Compression of DCONLY GRAY.
*
* Revision 1.11 2007/11/26 01:47:15 steve
* Add copyright notices per MS request.
*
* Revision 1.10 2007/11/08 02:52:32 steve
* Some progress in some encoding infrastructure.
*
* Revision 1.9 2007/11/07 18:11:02 steve
* Useful error message for bad magic numbers.
*
* Revision 1.8 2007/10/22 23:08:55 steve
* Protect prints with DETAILED_DEBUG.
*
* Revision 1.7 2007/09/18 17:02:49 steve
* Slight PNM header format change.
*
* Revision 1.6 2007/09/08 01:01:43 steve
* YUV444 color parses properly.
*
* Revision 1.5 2007/08/03 22:49:10 steve
* Do not write out the line padding.
*
* Revision 1.4 2007/07/30 23:09:57 steve
* Interleave FLEXBITS within HP block.
*
* Revision 1.3 2007/07/21 00:25:48 steve
* snapshot 2007 07 20
*
* Revision 1.2 2007/06/07 18:53:06 steve
* Parse HP coeffs that are all 0.
*
* Revision 1.1 2007/06/06 17:19:12 steve
* Introduce to CVS.
*
*/

