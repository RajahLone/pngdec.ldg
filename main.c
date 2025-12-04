
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
png_structp  png_read;
png_infop    img_info;

static png_uint_32 image_width;
static png_uint_32 image_height;
static png_uint_32 image_channels;
static png_uint_32 image_bitdepth;
static png_uint_32 image_coltype;
static png_uint_32 image_rowbytes;
static png_uint_32 frame_count;
static png_uint_32 play_count;

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

int32_t CDECL pngdec_open(png_bytep data, png_uint_32 size)
{
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
  if (png_read && img_info)
  {
    png_read_info(png_read, img_info);
    
    image_width    = png_get_image_width(png_read, img_info);
    image_height   = png_get_image_height(png_read, img_info);
    image_channels = png_get_channels(png_read, img_info);
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
    if (image_coltype == PNG_COLOR_TYPE_GRAY && image_bitdepth < 8)
    {
      png_set_expand_gray_1_2_4_to_8(png_read);
    }
    if (image_coltype == PNG_COLOR_TYPE_GRAY)
    {
      png_set_gray_to_rgb(png_read);
    }
    if (png_get_valid(png_read, img_info, PNG_INFO_tRNS))
    {
      png_set_tRNS_to_alpha(png_read);
    }
    if (image_coltype == PNG_COLOR_TYPE_RGB || image_coltype == PNG_COLOR_TYPE_GRAY || image_coltype == PNG_COLOR_TYPE_PALETTE)
    {
      png_set_filler(png_read, 0xFF, PNG_FILLER_AFTER);
    }
    
    png_set_swap_alpha(png_read); // RGBA to ARGB

    png_read_update_info(png_read, img_info);
    
    image_channels = png_get_channels(png_read, img_info);
    image_rowbytes = png_get_rowbytes(png_read, img_info);
      
    frame_count = 1;
    play_count = 0;
    
    if (png_get_valid(png_read, img_info, PNG_INFO_acTL))
    {
      png_get_acTL(png_read, img_info, &frame_count, &play_count);
    }
    
    return PNG_OK;
  }
      
  return PNG_ERROR;
}

uint32_t CDECL pngdec_get_width() { return (uint32_t)image_width; }
uint32_t CDECL pngdec_get_height() { return (uint32_t)image_height; }
uint32_t CDECL pngdec_get_channels() { return (uint32_t)image_channels; }
uint32_t CDECL pngdec_get_raster_size() { return (uint32_t)image_height * image_rowbytes; }
uint32_t CDECL pngdec_get_images_count() { return (uint32_t)frame_count; }
uint32_t CDECL pngdec_get_plays_count() { return (uint32_t)play_count; }

int32_t CDECL pngdec_get_image(int idx, unsigned char *p_frame, uint32_t *frame_left, uint32_t *frame_top, uint32_t *frame_width, uint32_t *frame_height, uint32_t *delay_num, uint32_t *delay_den, uint32_t *dispose_op, uint32_t *blend_op)
{
  uint32_t i, j;
  png_bytepp rows;

  if (png_read && img_info && (idx < frame_count))
  {
    rows = (png_bytepp)malloc(image_height * sizeof(png_bytep));
      
    if (p_frame && rows)
    {
      *frame_left = 0;
      *frame_top = 0;
      *frame_width = image_width;
      *frame_height = image_height;
 
      unsigned short  local_delay_num = 1;
      unsigned short  local_delay_den = 10;
      unsigned char   local_dispose_op = 0;
      unsigned char   local_blend_op = 0;

      for (j = 0; j < image_height; j++) { rows[j] = p_frame + (j * image_rowbytes); }

      if (png_get_valid(png_read, img_info, PNG_INFO_acTL))
      {
        for (i = 0; i < frame_count; i++)
        {
          if (i == idx)
          {
            png_read_frame_head(png_read, img_info);
            png_get_next_frame_fcTL(png_read, img_info, frame_width, frame_height, frame_left, frame_top, &local_delay_num, &local_delay_den, &local_dispose_op, &local_blend_op);
          
            png_read_image(png_read, rows);
          
            break;
          }
        }
      }
      else if (idx == 0)
      {
        png_read_image(png_read, rows);
      }
     
      *delay_num = local_delay_num;
      *delay_den = local_delay_den;
      *dispose_op = local_dispose_op;
      *blend_op = local_blend_op;
      
      free(rows);

      return PNG_OK;
    }
  }
      
  return PNG_ERROR;
}

int32_t CDECL pngdec_close()
{
  png_mf.data = NULL;
  png_mf.size = 0;
  png_mf.offset = 0;
  
  if (png_read && img_info)
  {
    //png_read_end(png_read, img_info); // no need and crashy
    png_destroy_read_struct(&png_read, &img_info, NULL);
  
    return PNG_OK;
  }
  return PNG_ERROR;
}

/* populate functions list and info for the LDG */

PROC LibFunc[] =
{
  {"pngdec_get_lib_version", "const char* pngdec_get_lib_version();\n", pngdec_get_lib_version},

  {"pngdec_open", "int32_t pngdec_open(uint8_t *data, const int size);\n", pngdec_open},
  {"pngdec_read", "int32_t pngdec_read();\n", pngdec_read},

  {"pngdec_get_width", "uint32_t pngdec_get_width();\n", pngdec_get_width},
  {"pngdec_get_height", "uint32_t pngdec_get_height();\n", pngdec_get_height},
  {"pngdec_get_raster_size", "uint32_t pngdec_get_raster_size();\n", pngdec_get_raster_size},
  {"pngdec_get_channels", "uint32_t pngdec_get_channels();\n", pngdec_get_channels},
  {"pngdec_get_images_count", "uint32_t pngdec_get_images_count();\n", pngdec_get_images_count},
  {"pngdec_get_plays_count", "uint32_t pngdec_get_plays_count();\n", pngdec_get_plays_count},
  
  {"pngdec_get_image", "int32_t pngdec_get_image(int idx, unsigned char *p_frame, uint32_t *frame_left, uint32_t *frame_top, uint32_t *frame_width, uint32_t *frame_height, uint32_t *delay_num, uint32_t *delay_den, uint32_t *dispose_op, uint32_t *blend_op);\n", pngdec_get_image},

  {"pngdec_close", "int32_t pngdec_close();\n", pngdec_close},

};

LDGLIB LibLdg[] = { { 0x0001, 11, LibFunc, VERSION_LDG(PNG_LIBPNG_VER_MAJOR, PNG_LIBPNG_VER_MINOR, PNG_LIBPNG_VER_RELEASE), 1} };

/*  */

int main(void)
{
  ldg_init(LibLdg);
  return 0;
}
