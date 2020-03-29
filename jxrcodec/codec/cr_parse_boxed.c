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
#pragma comment (user,"$Id: cr_parse_boxed.c,v 1.7 2012-03-18 21:47:07 thor Exp $")
#endif

#include "jxr_priv.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>

/*
** Generate a box-ID from the four-character identifier
*/
#define MAKE_ID(a,b,c,d) (((a) << 24) | ((b) << 16) | ((c) << 8) | ((d) << 0))

/*
** Read an unsigned long from the file. Return 0 (no kidding) on
** error. A real zero cannot be distinguished from this error,
** though there are no box type or box lengths for which a zero
** would be correct, and this is the only purpose of this function.
*/
static uint32_t read_ULONG(jxr_container_t c)
{
  unsigned char buffer[4];

  if (fread(buffer,1,sizeof(buffer),c->fd) != sizeof(buffer))
    return 0;

  return ((buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | (buffer[3] << 0));
}

/*
** Read the given box and fill the buffer, which is the given number
** of bytes large. Longer boxes will be simply truncated, though the
** default should be reasonable for all boxes this program needs.
** Returns the box type or zero on error.
**
** For superboxes only the box header is parsed off and the super box is
** entered.
*/
static uint32_t read_box(jxr_container_t c,unsigned char *buffer,size_t *bufsize)
{
  uint32_t size = read_ULONG(c);
  uint32_t type = read_ULONG(c);
  int seek      = 0;
  /* It might be that the size is one in which case the box exceeds the 64 bit
  ** limit. This is currently not supported here.
  */
  if (size < 8)
    return 0;

  if (type == MAKE_ID('j','p','l','h') || /* compositing layer header box */
      type == MAKE_ID('j','p','c','h') || /* codestream header box */
      type == MAKE_ID('j','p','2','h') || /* jp2 header box */
      type == MAKE_ID('c','g','r','g') || /* color group box */
      type == MAKE_ID('r','e','s',' ') || /* resolution box */
      type == MAKE_ID('u','i','n','f') || /* uuid info box */
      type == MAKE_ID('f','t','b','l') || /* fragment table box */
      type == MAKE_ID('c','o','m','p') || /* composition box */
      type == MAKE_ID('d','r','e','p') || /* desired reproduction box */
      type == MAKE_ID('j','p','2','c')) {  /* codestream box: Not a super box, but still not parsed here */
    /*
    ** There might be additional super boxes here, but since the content is
    ** then irrelevant to this program, just read them as normal boxes and
    ** ignore them.
    */
    *bufsize = size - 8; /* inner data, not box header */
    return type;
  }

  if (size >= *bufsize) {
    seek = size - 8 - *bufsize;
    size = *bufsize;
  }
  *bufsize = size;

  if (fread(buffer,1,size - 8,c->fd) != size - 8) {
    return 0;
  }

  if (seek > 0) {
    if (fseek(c->fd,seek,SEEK_CUR) != 0)
      return 0;
  }

  return type;
}

static int parse_ftyp(jxr_container_t c,const unsigned char *buffer,uint32_t size)
{
  int offset = 8; /* start of the compatibility list */

  if (size < 8 + 4 || memcmp(buffer,"jpx ",4))
    return JXR_EC_BADFORMAT; /* brand must be 15444-2 as this is a subset of it */
  
  while(offset + 4 < size + 8) {
    if (!memcmp(buffer+offset,"jxr0",4)) {
      c->profile_idc = 44; /* subbaseline profile */
      return 0; /* is acceptable */
    } else if (!memcmp(buffer+offset,"jxr1",4)) {
      c->profile_idc = 55; /* baseline profile */
      return 0;
    } else if (!memcmp(buffer+offset,"jxr2",4)) {
      c->profile_idc = 66; /* main profile */
      return 0;
    } else if (!memcmp(buffer+offset,"jxrc",4)) {
      c->profile_idc = 111; /* advanced profile */
      return 0;
    }
    offset += 4;
  }

  /* not compatible to anything I understand */
  return JXR_EC_BADFORMAT;
}

/*
** Parse off the image header box
*/
static int parse_ihdr(jxr_container_t c,const unsigned char *buffer,size_t boxsize)
{
  uint32_t width,height,depth,bpc;

  if (boxsize != 8 + 4 + 4 + 2 + 1 + 1 + 1 + 1)
    return JXR_EC_BADFORMAT; 

  width  = (buffer[4] << 24) | (buffer[5] << 16) | (buffer[6] << 8) | (buffer[7] << 0);
  height = (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | (buffer[3] << 0);
  depth  = (buffer[8] <<  8) | (buffer[9] << 0);
  bpc    = buffer[10];

  if (buffer[11] != 11) /* must be JPEGXR */
    return JXR_EC_BADFORMAT; 

  if (c->wid && c->wid != width) /* must be consistent */
    return JXR_EC_FEATURE_NOT_IMPLEMENTED;
  c->wid = width;

  if (c->hei && c->hei != height)
    return JXR_EC_FEATURE_NOT_IMPLEMENTED;
  c->hei = height;
  
  switch(c->c_idx) {
  case 0:
  case 1: /* override is possible */
    c->depth = depth; /* default depth */
    break;
  case 2: /* alpha channel */
    if (depth != 1)
      return JXR_EC_FEATURE_NOT_IMPLEMENTED;
    c->depth++; /* include the alpha channel */
    c->separate_alpha_image_plane = 1;
    break;
  }

  if (bpc != 255) {
    if (c->bpp && c->bpp != (bpc & 0x7f) + 1)
      return JXR_EC_FEATURE_NOT_IMPLEMENTED;
    c->bpp = (bpc & 0x7f) + 1;
  }

  return 0;
}

/*
** Parse off the number of bits per component box
** There is only one reason why it can be here: To
** support the 565 mode. Alll other modes have
** a consistent number of bits per component.
*/
static int parse_bpcc(jxr_container_t c,const unsigned char *buffer,size_t size)
{
  uint32_t i;

  if (size <= 8)
    return JXR_EC_BADFORMAT;

  size -= 8;
  
  if (size == 3) {
    if (buffer[0] == 4 && buffer[1] == 5 && buffer[2] == 4) {
      if (c->bpp && c->bpp != 6)
	return JXR_EC_FEATURE_NOT_IMPLEMENTED;
      c->bpp = 6; /* 565 mode */
      return 0;
    }
  }
  /*
  ** Here all bit depths must be identical, XR is too limited... 
  */
  for(i = 0;i < size;i++) {
    if (buffer[0] != buffer[i])
      return JXR_EC_FEATURE_NOT_IMPLEMENTED;
  }
  if (c->bpp && c->bpp != (buffer[0] & 0x7f) + 1)
    return JXR_EC_FEATURE_NOT_IMPLEMENTED;
  c->bpp = (buffer[0] & 0x7f) + 1;

  return 0;
}

/*
** Parse the color specification box if we have one
*/
static int parse_colr(jxr_container_t c,const unsigned char *buffer,size_t boxsize)
{
  if (boxsize < 7 + 8)
    return JXR_EC_BADFORMAT;

  /*
  ** All color specification methods except enumerated are simply ignored here
  */
  if (buffer[0] == 1) { /* enumerated method */
    int prec = buffer[1];
    if (prec >= 128)
      prec -= 256; /* binary complement, is signed not unsigned */
    prec += 128;   /* make unsigned */
    if (prec > c->cprec) {
      /* consider this color instead */
      c->cprec = prec;
      c->color = (buffer[3] << 24) | (buffer[4] << 16) | (buffer[5] << 8) | (buffer[6] << 0);
    }
  }
  return 0;
}

/*
** Parse off the channel definition box 
*/
static int parse_cdef(jxr_container_t c,const unsigned char *buffer,size_t boxsize)
{
  int channels,i;
  
  if (boxsize < 4 + 8)
    return JXR_EC_BADFORMAT;
  
  channels = (buffer[0] << 8) | (buffer[1] << 0);
  if (boxsize != (channels * 3 + 1) * 2 + 8)
    return JXR_EC_BADFORMAT;

  if (c->channels && c->channels != channels)
    return JXR_EC_BADFORMAT; /* must be consistent */

  c->channels = channels;
  for(i = 0;i < channels;i++) {
    int cidx = (buffer[2 + i * 6] << 8) | (buffer[3 + i * 6] << 0);
    int ctyp = (buffer[4 + i * 6] << 8) | (buffer[5 + i * 6] << 0);
    int asoc = (buffer[6 + i * 6] << 8) | (buffer[7 + i * 6] << 0);
    if (cidx == channels-1) {
      /* The only channel that could be an alpha channel. Comes always last here */
      if (ctyp == 1 || ctyp == 2) {
	c->alpha = ctyp;
	if (asoc != 0) /* Only if associated with all of the image */
	  return JXR_EC_FEATURE_NOT_IMPLEMENTED;
	continue;
      }
    }
    /* Here: Must be a standard channel */
    if (ctyp != 0)
      return JXR_EC_FEATURE_NOT_IMPLEMENTED;
    /* Association must be the canonical, channel reordering not supported here. */
    if (asoc != cidx + 1)
      return JXR_EC_FEATURE_NOT_IMPLEMENTED;
  }

  return 0;
}

static int parse_pxfm(jxr_container_t c,const unsigned char *buffer,size_t boxsize)
{
  int channels,i,curtype = -1;
  
  if (boxsize < 4 + 8)
    return JXR_EC_BADFORMAT;

  channels = (buffer[0] << 8) | (buffer[1] << 0);

  if (boxsize != channels * 4 + 2 + 8)
    return JXR_EC_BADFORMAT;

  if (c->channels && c->channels != channels)
    return JXR_EC_BADFORMAT; /* must be consistent */

  c->channels = channels;
  for(i = 0;i < channels;i++) {
    int channel = (buffer[2 + i * 4] << 8) + (buffer[3 + i * 4] << 0);
    int type    = (buffer[4 + i * 4] << 8) + (buffer[5 + i * 4] << 0);

    if (((type == 0x1000 && channel < 3) || (type == 0x2000 && channel == 3)) && channels == 4) {
      if (curtype == -1 || curtype == 0x1000) {
	curtype = 0x1000; /* RGBE */
      } else {
	return JXR_EC_FEATURE_NOT_IMPLEMENTED; /* Something else not supported here */
      }
    } else if (type == 0x400a) {
      if (curtype == -1 || curtype == 0x400a) {
	curtype = 0x400a; /* half float */
      } else {
	return JXR_EC_FEATURE_NOT_IMPLEMENTED; /* Something else not supported here */
      }
    } else if (type == 0x4017) {
      if (curtype == -1 || curtype == 0x4017) {
	curtype = 0x4017; /* single precision float */
      } else {
	return JXR_EC_FEATURE_NOT_IMPLEMENTED;
      }
    } else if (type == 0x300d) {
      if (curtype == -1 || curtype == 0x300d) {
	curtype = 0x300d; /* fixpoint with 13 fractional bits */
      } else {
	return JXR_EC_FEATURE_NOT_IMPLEMENTED; 
      }
    } else if (type == 0x3018) {
      if (curtype == -1 || curtype == 0x3018) {
	curtype = 0x3018; /* fixpoint with 24 fractional bits */
      } else {
	return JXR_EC_FEATURE_NOT_IMPLEMENTED;
      }
    } else if (type == 0) {
      if (curtype == -1 || curtype == 0) {
	curtype = 0; /* integer */
      } else {
	return JXR_EC_FEATURE_NOT_IMPLEMENTED;
      }
    } else {
      return JXR_EC_FEATURE_NOT_IMPLEMENTED; /* everything else is not supported here */
    }
  }

  c->pixeltype = curtype;
  
  return 0;
}
/*
** Parse the image header superbox which provides defaults
** for all codestreams
*/
static int parse_jp2h(jxr_container_t c,size_t boxsize)
{
  unsigned char buffer[256];
  size_t size;
  uint32_t type;
  int rc;
  int have_ihdr = 0;
  int have_bpcc = 0;
  int have_colr = 0;
  int have_cdef = 0;
  int have_pxfm = 0;
  int have_res  = 0;

  do {
    size = sizeof(buffer);
    type = read_box(c,buffer,&size);
    if (type == MAKE_ID('i','h','d','r')) {
      if (have_ihdr)
	return JXR_EC_BADFORMAT;
      have_ihdr = 1;
      rc = parse_ihdr(c,buffer,size);
      if (rc) return rc;
    } else if (type == MAKE_ID('b','p','c','c')) {
      if (have_bpcc)
	return JXR_EC_BADFORMAT;
      have_bpcc = 1;
      rc = parse_bpcc(c,buffer,size);
      if (rc) return rc;
    } else if (type == MAKE_ID('c','o','l','r')) {
      /* No color group box, thus only one color box */
      if (have_colr)
	return JXR_EC_BADFORMAT;
      have_colr = 1;
      rc = parse_colr(c,buffer,size);
      if (rc) return rc;
    } else if (type == MAKE_ID('p','c','l','r')) {
      /* Palette mapping is currently not implemented */
      return JXR_EC_FEATURE_NOT_IMPLEMENTED;
    } else if (type == MAKE_ID('c','m','a','p')) {
      /* Same difference, cmap exists only for palette mapping */
      return JXR_EC_FEATURE_NOT_IMPLEMENTED;
    } else if (type == MAKE_ID('c','d','e','f')) {
      /* Channel definition box */
      if (have_cdef)
	return JXR_EC_BADFORMAT;
      have_cdef = 1;
      rc = parse_cdef(c,buffer,size);
      if (rc) return rc;
    } else if (type == MAKE_ID('p','x','f','m')) {
      if (have_pxfm)
	return JXR_EC_BADFORMAT;
      have_pxfm = 1;
      rc = parse_pxfm(c,buffer,size);
      if (rc) return rc;
    } else if (type == MAKE_ID('r','e','s',' ')) {
      /* Resolution box. Currently ignored, skip over the superbox */
      if (have_res)
	return JXR_EC_BADFORMAT;
      have_res = 1;
      if (fseek(c->fd,size,SEEK_CUR) != 0)
	return JXR_EC_IO;
    }
    /* All other boxes are ignored. */
    if (boxsize < size)
      return JXR_EC_BADFORMAT;
    boxsize -= size;
  } while(boxsize);

  return 0;
}

/*
** Parse the codestream header superbox which provides settings
** for a specific codestream
*/
static int parse_jpch(jxr_container_t c,size_t boxsize)
{
  unsigned char buffer[256];
  size_t size;
  uint32_t type;
  int rc;
  int have_ihdr = 0;
  int have_bpcc = 0;

  do {
    size = sizeof(buffer);
    type = read_box(c,buffer,&size);
    if (type == MAKE_ID('i','h','d','r')) {
      if (have_ihdr)
	return JXR_EC_BADFORMAT;
      have_ihdr = 1;
      rc = parse_ihdr(c,buffer,size);
      if (rc) return rc;
    } else if (type == MAKE_ID('b','p','c','c')) {
      if (have_bpcc)
	return JXR_EC_BADFORMAT;
      have_bpcc = 1;
      rc = parse_bpcc(c,buffer,size);
      if (rc) return rc;
    } else if (type == MAKE_ID('p','c','l','r')) {
      /* Palette mapping is currently not implemented */
      return JXR_EC_FEATURE_NOT_IMPLEMENTED;
    } else if (type == MAKE_ID('c','m','a','p')) {
      /* Same difference, cmap exists only for palette mapping */
      return JXR_EC_FEATURE_NOT_IMPLEMENTED;
    }
    /* All other boxes are ignored. */
    if (boxsize < size)
      return JXR_EC_BADFORMAT;
    boxsize -= size;
  } while(boxsize);

  return 0;
}

/*
** Parse the codestream registration box
*/
static int parse_creg(jxr_container_t c,const unsigned char *buffer,uint32_t size)
{
  if (size < 10 + 8)
    return JXR_EC_BADFORMAT;

  /* The grid size is actually pretty irrelevant as long as the resolutions and
  ** offsets are all identical. Scaling is not supported by this simple code.
  */
  size   -= 4 + 8;
  buffer += 4;
  while(size) {
    int cdn;
    if (size < 6)
      return JXR_EC_BADFORMAT;
    cdn = (buffer[0] << 8) | (buffer[1]);
    if (cdn > 1) /* At most two codestreams supported */
      return JXR_EC_FEATURE_NOT_IMPLEMENTED;
    if (buffer[2] != 1 || buffer[3] != 1 || buffer[4] != 0 || buffer[5] != 0)
      return JXR_EC_FEATURE_NOT_IMPLEMENTED;
    buffer += 6;
    size   -= 6;
  }
  return 0;
}

/*
** Parse the opacity box. This is an alternative to the channel definition box
** and somewhat simpler.
*/
static int parse_opct(jxr_container_t c,const unsigned char *buffer,size_t size)
{
  if (size < 1 + 8)
    return JXR_EC_BADFORMAT;
  /*
  ** Chroma-keying is not supported.
  */
  if (size > 1 + 8)
    return JXR_EC_FEATURE_NOT_IMPLEMENTED;

  switch(buffer[0]) {
  case 0:
    /* Standard opacity. */
    c->alpha = 1;
    break;
  case 1:
    /* Premultiplied opacity. */
    c->alpha = 2;
    break;
  case 2: /* chroma key */
    return JXR_EC_FEATURE_NOT_IMPLEMENTED;
  }

  return JXR_EC_BADFORMAT;
}

/*
** Parse the color group super box. Contains only color boxes
*/
static int parse_cgrp(jxr_container_t c,size_t boxsize)
{ 
  unsigned char buffer[256];
  size_t size;
  uint32_t type;
  int rc;

  do {
    size = sizeof(buffer);
    type = read_box(c,buffer,&size);
    if (type == MAKE_ID('c','o','l','r')) {
      rc = parse_colr(c,buffer,size);
      if (rc) return rc;
    } else {
      return JXR_EC_BADFORMAT;
    }
    if (boxsize < size)
      return JXR_EC_BADFORMAT;
    boxsize -= size;
  } while(boxsize);

  return 0;
}

/*
** Parse the compositing layer header box.
*/
static int parse_jplh(jxr_container_t c,size_t boxsize)
{ 
  unsigned char buffer[256];
  size_t size;
  uint32_t type;
  int rc;
  int have_creg = 0;
  int have_cgrp = 0;
  int have_opct = 0;
  int have_cdef = 0;
  int have_pxfm = 0;
  int have_res  = 0;

  do {
    size = sizeof(buffer);
    type = read_box(c,buffer,&size);
    if (type == MAKE_ID('c','r','e','g')) {
      if (have_creg)
	return JXR_EC_BADFORMAT;
      have_creg = 1;
      rc = parse_creg(c,buffer,size);
      if (rc) return rc;
    } else if (type == MAKE_ID('c','d','e','f')) {
      if (have_cdef || have_opct)
	return JXR_EC_BADFORMAT;
      have_cdef = 1;
      rc = parse_cdef(c,buffer,size);
      if (rc) return rc; 
    } else if (type == MAKE_ID('p','x','f','m')) {
      if (have_pxfm)
	return JXR_EC_BADFORMAT;
      have_pxfm = 1;
      rc = parse_pxfm(c,buffer,size);
      if (rc) return rc;
    } else if (type == MAKE_ID('o','p','c','t')) {
        if (have_cdef || have_opct)
	return JXR_EC_BADFORMAT;
      have_opct = 1;
      rc = parse_opct(c,buffer,size);
      if (rc) return rc;
    } else if (type == MAKE_ID('c','g','r','p')) {
      /* A color group box */
      if (have_cgrp)
	return JXR_EC_BADFORMAT;
      have_cgrp = 1;
      rc = parse_cgrp(c,size);
      if (rc) return rc;
    } else if (type == MAKE_ID('r','e','s',' ')) {
      /* Resolution box. Currently ignored, skip over the superbox */
      if (have_res)
	return JXR_EC_BADFORMAT;
      have_res = 1;
      if (fseek(c->fd,size,SEEK_CUR) != 0)
	return JXR_EC_IO;
    }
    /* All other boxes are ignored. */
    if (boxsize < size)
      return JXR_EC_BADFORMAT;
    boxsize -= size;
  } while(boxsize);

  return 0;
}

int jxr_read_image_container_boxed(jxr_container_t c, FILE*fd)
{
  unsigned char buffer[256];
  size_t size;
  uint32_t type;
  int rc;
  int have_ftyp = 0;
  int have_jp2h = 0;
  int have_jplh = 0;

  c->fd = fd;
  c->color = -1; /* Still unknown */

  do {
    size = sizeof(buffer);
    type = read_box(c,buffer,&size);
    if (size == 0 && !feof(c->fd))
      return JXR_EC_IO;
    if (type == MAKE_ID('f','t','y','p')) {
      /* File type box */
      if (have_ftyp)
	return JXR_EC_BADFORMAT;
      have_ftyp = 1;
      rc = parse_ftyp(c,buffer,size);
      if (rc != 0)
	return rc;
    } else if (type == MAKE_ID('j','p','2','h')) {
      /* JP2 header box. Since this is a super box
       * the data remained in the file */
      if (have_jp2h)
	return JXR_EC_BADFORMAT;
      have_jp2h = 1;
      rc = parse_jp2h(c,size);
      if (rc != 0)
	return rc;
    } else if (type == MAKE_ID('j','p','c','h')) {
      /* Codestream header box */
      if (++c->c_idx > 2)
	return JXR_EC_FEATURE_NOT_IMPLEMENTED; /* Not more than two codestreams supported here */
      rc = parse_jpch(c,size);
      if (rc != 0)
	return rc;
    } else if (type == MAKE_ID('j','p','l','h')) {
      /* Compositing layer header box */
      if (have_jplh)
	return JXR_EC_FEATURE_NOT_IMPLEMENTED; /* layer composition not supported here, only one layer */
      have_jplh = 1;
      rc = parse_jplh(c,size);
    } else if (type == MAKE_ID('j','p','2','c')) {
      /* The codestream itself. There can be two of them, one regular and one alpha stream */
      if (c->image_offset) {
	/* Is already the second. */
	if (c->alpha_offset) {
	  /* More than two are not supported. */
	  return JXR_EC_FEATURE_NOT_IMPLEMENTED; /* layer composition not supported here, only one layer */
	}
	c->alpha_offset = ftell(c->fd);
	c->alpha_size   = size;
      } else {
	c->image_offset = ftell(c->fd);
	c->image_size   = size;
      }
      /* Seek over it. */
      if (fseek(c->fd,size,SEEK_CUR) != 0)
	return JXR_EC_IO;
      c->image_count++;
    } else if (type == MAKE_ID('u','i','n','f') || /* uuid info box */
	       type == MAKE_ID('f','t','b','l') || /* fragment table box */
	       type == MAKE_ID('c','o','m','p') || /* composition box */
	       type == MAKE_ID('d','r','e','p')) { /* desired reproduction box */
      /* Super boxes we currently don't need - seek over them */
      if (fseek(c->fd,size,SEEK_CUR) != 0)
	return JXR_EC_IO;
    }
  } while(!feof(c->fd));

  /*
  ** check whether there are as many channels as we have components. If not
  ** the result is not supported.
  */
  if (c->channels != c->depth)
    return JXR_EC_BADFORMAT;

  return 0;
}
