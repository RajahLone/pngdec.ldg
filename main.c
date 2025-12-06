
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <gem.h>
#include <ldg.h>
#include <png.h>

#define STRINGIFY(x) #x
#define VERSION_LIB(A,B,C) STRINGIFY(A) "." STRINGIFY(B) "." STRINGIFY(C)
#define VERSION_LDG(A,B,C) "APNG decoder from the PNGLIB (" STRINGIFY(A) "." STRINGIFY(B) "." STRINGIFY(C) ")"

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#define PNG_ERROR 0
#define PNG_OK 1
#define PNG_BYTES_TO_CHECK 8

/* structures */

typedef struct png_mem_file {
  png_bytep data;
  png_uint_32 size;
  png_uint_32 offset;
} png_mem_file;

/* global variables */

png_mem_file png_mf;
png_structp  png_read = NULL;
png_infop    img_info;

static png_uint_32 image_width;
static png_uint_32 image_height;
static png_uint_32 image_channels;
static png_uint_32 image_bitdepth;
static png_uint_32 image_coltype;
static png_uint_32 image_rowbytes;
static png_uint_32 frames_count;
static png_uint_32 plays_count;

static png_uint_32 frame_left;
static png_uint_32 frame_top;
static png_uint_32 frame_width;
static png_uint_32 frame_height;
static unsigned short frame_delay_num;
static unsigned short frame_delay_den;
static unsigned char frame_dispose;
static unsigned char frame_blend;

/* internal functions */

static void pngldg_read(png_struct *png_ptr, png_bytep data, png_size_t count)
{
  png_mem_file *mf = (png_mem_file *)png_get_io_ptr(png_ptr);
  
  count = MIN(count, mf->size - mf->offset);
  
  if (count < 1) { return; }
  
  if (mf->offset + count <= mf->size)
  {
    memcpy(data, mf->data + mf->offset, count);
    mf->offset += count;
  }
}

void *pngldg_malloc(png_struct *png_ptr, png_alloc_size_t size) { return ldg_Malloc(size); }
void pngldg_free(png_struct *png_ptr, void *ptr) { ldg_Free(ptr); }

/* functions */

const char * CDECL pngdec_get_lib_version() { return VERSION_LIB(PNG_LIBPNG_VER_MAJOR, PNG_LIBPNG_VER_MINOR, PNG_LIBPNG_VER_RELEASE); }

int32_t CDECL pngdec_close()
{
  png_mf.data = NULL;
  png_mf.size = 0;
  png_mf.offset = 0;
  
  image_width = 0;
  image_height = 0;
  image_channels = 0;
  image_bitdepth = 0;
  image_coltype = 0;
  image_rowbytes = 0;
  frames_count = 1;
  plays_count = 0;

  frame_left = 0;
  frame_top = 0;
  frame_width = 0;
  frame_height = 0;
  frame_delay_num = 1;
  frame_delay_den = 10;
  frame_dispose = 0;
  frame_blend = 0;

  if (png_read && img_info)
  {
    //png_read_end(png_read, img_info); // no need and crashy
    png_destroy_read_struct(&png_read, &img_info, NULL);
  
    png_read = NULL;
    
    return PNG_OK;
  }
  return PNG_ERROR;
}
int32_t CDECL pngdec_open(png_bytep data, png_uint_32 size)
{
  if (png_read != NULL) { pngdec_close(); }
  
  png_mf.data = data;
  png_mf.size = size;
  png_mf.offset = 0;

  if (png_sig_cmp(data, 0, PNG_BYTES_TO_CHECK) == 0)
  {
    png_read = png_create_read_struct_2(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL, NULL, pngldg_malloc, pngldg_free);
    img_info = png_create_info_struct(png_read);

    if (png_read && img_info)
    {
      if (setjmp(png_jmpbuf(png_read))) { png_destroy_read_struct(&png_read, &img_info, NULL); return PNG_ERROR; }

      png_set_read_fn(png_read, &png_mf, pngldg_read);
      // crash if png_set_sig_bytes() called
      
      return PNG_OK;
    }
  }
  
  return PNG_ERROR;
}

int32_t CDECL pngdec_read()
{
  if (png_read == NULL) { return PNG_ERROR; }

  if (png_read && img_info)
  {
    png_read_info(png_read, img_info);
    
    image_width    = png_get_image_width(png_read, img_info);
    image_height   = png_get_image_height(png_read, img_info);
    image_bitdepth = png_get_bit_depth(png_read, img_info);
    image_coltype  = png_get_color_type(png_read, img_info);
          
    if (image_bitdepth == 16)
    {
      png_set_scale_16(png_read);
    }
    if (image_coltype == PNG_COLOR_TYPE_PALETTE)
    {
      png_set_palette_to_rgb(png_read);
    }
    else if (image_coltype == PNG_COLOR_TYPE_GRAY)
    {
      if (image_bitdepth < 8)
      {
        png_set_expand_gray_1_2_4_to_8(png_read);
      }
      png_set_gray_to_rgb(png_read);
    }
    if (png_get_valid(png_read, img_info, PNG_INFO_tRNS))
    {
      png_set_tRNS_to_alpha(png_read);
    }
    if (image_coltype == PNG_COLOR_TYPE_RGB || image_coltype == PNG_COLOR_TYPE_GRAY || image_coltype == PNG_COLOR_TYPE_PALETTE)
    {
      png_set_add_alpha(png_read, 0xFF, PNG_FILLER_BEFORE); // put A before RGB
    }
    else if (image_coltype == PNG_COLOR_TYPE_RGB_ALPHA)
    {
      png_set_swap_alpha(png_read); // RGBA to ARGB
    }
    
    png_read_update_info(png_read, img_info);

    image_bitdepth = png_get_bit_depth(png_read, img_info);
    image_coltype  = png_get_color_type(png_read, img_info);
    image_channels = png_get_channels(png_read, img_info);
    image_rowbytes = png_get_rowbytes(png_read, img_info);
      
    frames_count = 1;
    plays_count = 0;
    
    if (png_get_valid(png_read, img_info, PNG_INFO_acTL))
    {
      png_get_acTL(png_read, img_info, &frames_count, &plays_count);
    }
    
    return PNG_OK;
  }
      
  return PNG_ERROR;
}

uint32_t CDECL pngdec_get_width()
{
  if (png_read == NULL) { return PNG_ERROR; }
  return (uint32_t)image_width;
}
uint32_t CDECL pngdec_get_height()
{
  if (png_read == NULL) { return PNG_ERROR; }
  return (uint32_t)image_height;
}
uint32_t CDECL pngdec_get_channels()
{
  if (png_read == NULL) { return PNG_ERROR; }
  return (uint32_t)image_channels;
}
uint32_t CDECL pngdec_get_color_type()
{
  if (png_read == NULL) { return PNG_ERROR; }
  return (uint32_t)image_coltype;
}
uint32_t CDECL pngdec_get_bitdepth()
{
  if (png_read == NULL) { return PNG_ERROR; }
  return (uint32_t)image_bitdepth;
}
uint32_t CDECL pngdec_get_rowbytes()
{
  if (png_read == NULL) { return PNG_ERROR; }
  return (uint32_t)image_rowbytes;
}
uint32_t CDECL pngdec_get_frames_count()
{
  if (png_read == NULL) { return PNG_ERROR; }
  return (uint32_t)frames_count;
}
uint32_t CDECL pngdec_get_plays_count()
{
  if (png_read == NULL) { return PNG_ERROR; }
  return (uint32_t)plays_count;
}

int32_t CDECL pngdec_get_frame(int idx, unsigned char *p_frame)
{
  if (png_read == NULL) { return PNG_ERROR; }

  uint32_t i, j;
  png_bytepp rows;

  if (png_read && img_info && (idx < frames_count))
  {
    frame_left = 0;
    frame_top = 0;
    frame_width = image_width;
    frame_height = image_height;
    frame_delay_num = 1;
    frame_delay_den = 10;
    frame_dispose = 0;
    frame_blend = 0;

    rows = (png_bytepp)png_malloc(png_read, image_height * sizeof(png_bytep));
      
    if (p_frame && rows)
    {
      for (j = 0; j < image_height; j++) { rows[j] = p_frame + (j * image_rowbytes); }

      if (png_get_valid(png_read, img_info, PNG_INFO_acTL))
      {
        for (i = 0; i < frames_count; i++)
        {
          if (i == idx)
          {
            png_read_frame_head(png_read, img_info);
            png_get_next_frame_fcTL(png_read, img_info, &frame_width, &frame_height, &frame_left, &frame_top, &frame_delay_num, &frame_delay_den, &frame_dispose, &frame_blend);
          
            png_read_image(png_read, rows);
          
            break;
          }
        }
      }
      else if (idx == 0)
      {
        png_read_image(png_read, rows);
      }
      else
      {
        png_free(png_read, rows);
        return PNG_ERROR;
      }
      
      png_free(png_read, rows);

      return PNG_OK;
    }
  }
  return PNG_ERROR;
}

uint32_t CDECL pngdec_get_frame_left() { if (png_read == NULL) { return PNG_ERROR; }
return (uint32_t)frame_left; }
uint32_t CDECL pngdec_get_frame_top()
{
  if (png_read == NULL) { return PNG_ERROR; }
  return (uint32_t)frame_top;
}
uint32_t CDECL pngdec_get_frame_width()
{
  if (png_read == NULL) { return PNG_ERROR; }
  return (uint32_t)frame_width;
}
uint32_t CDECL pngdec_get_frame_height()
{
  if (png_read == NULL) { return PNG_ERROR; }
  return (uint32_t)frame_height;
}
uint32_t CDECL pngdec_get_frame_delay_num()
{
  if (png_read == NULL) { return PNG_ERROR; }
  return (uint32_t)frame_delay_num;
}
uint32_t CDECL pngdec_get_frame_delay_den()
{
  if (png_read == NULL) { return PNG_ERROR; }
  return (uint32_t)frame_delay_den;
}
uint32_t CDECL pngdec_get_frame_dispose()
{
  if (png_read == NULL) { return PNG_ERROR; }
  return (uint32_t)frame_dispose;
}
uint32_t CDECL pngdec_get_frame_blend()
{
  if (png_read == NULL) { return PNG_ERROR; }
  return (uint32_t)frame_blend;
}


/* populate functions list and info for the LDG */

PROC LibFunc[] =
{
  {"pngdec_get_lib_version", "const char* pngdec_get_lib_version();\n", pngdec_get_lib_version},

  {"pngdec_open", "int32_t pngdec_open(uint8_t *data, const int size);\n", pngdec_open},
  {"pngdec_read", "int32_t pngdec_read();\n", pngdec_read},

  {"pngdec_get_width", "uint32_t pngdec_get_width();\n", pngdec_get_width},
  {"pngdec_get_height", "uint32_t pngdec_get_height();\n", pngdec_get_height},
  {"pngdec_get_channels", "uint32_t pngdec_get_channels();\n", pngdec_get_channels},
  {"pngdec_get_color_type", "uint32_t pngdec_get_color_type();\n", pngdec_get_color_type},
  {"pngdec_get_bitdepth", "uint32_t pngdec_get_bitdepth();\n", pngdec_get_bitdepth},
  {"pngdec_get_rowbytes", "uint32_t pngdec_get_rowbytes();\n", pngdec_get_rowbytes},
  {"pngdec_get_frames_count", "uint32_t pngdec_get_frames_count();\n", pngdec_get_frames_count},
  {"pngdec_get_plays_count", "uint32_t pngdec_get_plays_count();\n", pngdec_get_plays_count},
  
  {"pngdec_get_frame", "int32_t pngdec_get_frame(int idx, unsigned char *p_frame);\n", pngdec_get_frame},

  {"pngdec_get_frame_left", "uint32_t pngdec_get_frame_left();\n", pngdec_get_frame_left},
  {"pngdec_get_frame_top", "uint32_t pngdec_get_frame_top();\n", pngdec_get_frame_top},
  {"pngdec_get_frame_width", "uint32_t pngdec_get_frame_width();\n", pngdec_get_frame_width},
  {"pngdec_get_frame_height", "uint32_t pngdec_get_frame_height();\n", pngdec_get_frame_height},
  {"pngdec_get_frame_delay_num", "uint32_t pngdec_get_frame_delay_num();\n", pngdec_get_frame_delay_num},
  {"pngdec_get_frame_delay_den", "uint32_t pngdec_get_frame_delay_den();\n", pngdec_get_frame_delay_den},
  {"pngdec_get_frame_dispose", "uint32_t pngdec_get_frame_dispose();\n", pngdec_get_frame_dispose},
  {"pngdec_get_frame_blend", "uint32_t pngdec_get_frame_blend();\n", pngdec_get_frame_blend},

  {"pngdec_close", "int32_t pngdec_close();\n", pngdec_close},
};

LDGLIB LibLdg[] = { { 0x0001, 21, LibFunc, VERSION_LDG(PNG_LIBPNG_VER_MAJOR, PNG_LIBPNG_VER_MINOR, PNG_LIBPNG_VER_RELEASE), 1} };

/*  */

int main(void)
{
  ldg_init(LibLdg);
  return 0;
}
