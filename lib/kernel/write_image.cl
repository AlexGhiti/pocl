/* OpenCL built-in library: write_image()

   Copyright (c) 2013 Ville Korhonen 
   
   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:
   
   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.
   
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.
*/

#include "templates.h"
#include "pocl_image_rw_utils.h"

static uint4
map_channels (uint4 color, int order)
{
  switch (order)
    {
    case CL_ARGB:
      return color.wxyz;
    case CL_BGRA:
      return color.zyxw;
    case CL_RGBA:
    default:
      return color;
    }
}

/* only for CL_FLOAT, CL_SNORM_INT8, CL_UNORM_INT8,
 * CL_SNORM_INT16, CL_UNORM_INT16 channel types */
static void
write_float4_pixel (float4 color, void *data, size_t base_index, int type)
{
  if (type == CL_FLOAT)
    {
      ((float4 *)data)[base_index] = color;
      return;
    }
  if (type == CL_HALF_FLOAT)
    {
#if !defined(LLVM_OLDER_THAN_3_9) && __has_builtin(__builtin_convertvector)
      typedef float vector4float __attribute__ ((__vector_size__ (16)));
      typedef half vector4half __attribute__ ((__vector_size__ (8)));
      vector4half vh = __builtin_convertvector(color, vector4half);
      ((vector4half *)data)[base_index] = vh;
      return;
#else
      __builtin_trap ();
#endif
    }
  const float4 f127 = ((float4) (SCHAR_MAX));
  const float4 f32767 = ((float4) (SHRT_MAX));
  const float4 f255 = ((float4) (UCHAR_MAX));
  const float4 f65535 = ((float4) (USHRT_MAX));
  if (type == CL_SNORM_INT8)
    {
      /*  <-1.0, 1.0> to <I*_MIN, I*_MAX> */
      float4 colorf = color * f127;
      char4 final_color = convert_char4_sat_rte (colorf);
      ((char4 *)data)[base_index] = final_color;
      return;
    }
  if (type == CL_SNORM_INT16)
    {
      float4 colorf = color * f32767;
      short4 final_color = convert_short4_sat_rte (colorf);
      ((short4 *)data)[base_index] = final_color;
      return;
    }
  if (type == CL_UNORM_INT8)
    {
      /* <0, I*_MAX> to <0.0, 1.0> */
      /*  <-1.0, 1.0> to <I*_MIN, I*_MAX> */
      float4 colorf = color * f255;
      uchar4 final_color = convert_uchar4_sat_rte (colorf);
      ((uchar4 *)data)[base_index] = final_color;
      return;
    }
  if (type == CL_UNORM_INT16)
    {
      float4 colorf = color * f65535;
      ushort4 final_color = convert_ushort4_sat_rte (colorf);
      ((ushort4 *)data)[base_index] = final_color;
      return;
    }

  return;
}

/* only for CL_FLOAT, CL_SNORM_INT8, CL_UNORM_INT8,
 * CL_SNORM_INT16, CL_UNORM_INT16 channel types */
static void
write_float_pixel (float color, void *data, size_t base_index, int type)
{
  if (type == CL_FLOAT)
    {
      ((float *)data)[base_index] = color;
      return;
    }
  const float f127 = ((float)SCHAR_MAX);
  const float f32767 = ((float)SHRT_MAX);
  const float f255 = ((float)UCHAR_MAX);
  const float f65535 = ((float)USHRT_MAX);
  if (type == CL_SNORM_INT8)
    {
      /*  <-1.0, 1.0> to <I*_MIN, I*_MAX> */
      float colorf = color * f127;
      char final_color = convert_char_sat_rte (colorf);
      ((char *)data)[base_index] = final_color;
      return;
    }
  if (type == CL_SNORM_INT16)
    {
      float colorf = color * f32767;
      short final_color = convert_short_sat_rte (colorf);
      ((short *)data)[base_index] = final_color;
      return;
    }
  if (type == CL_UNORM_INT8)
    {
      /* <0, I*_MAX> to <0.0, 1.0> */
      /*  <-1.0, 1.0> to <I*_MIN, I*_MAX> */
      float colorf = color * f255;
      uchar final_color = convert_uchar_sat_rte (colorf);
      ((uchar *)data)[base_index] = final_color;
      return;
    }
  if (type == CL_UNORM_INT16)
    {
      float colorf = color * f65535;
      ushort final_color = convert_ushort_sat_rte (colorf);
      ((ushort *)data)[base_index] = final_color;
      return;
    }

  return;
}

/* for use inside filter functions
 * no channel mapping
 * no pointers to img metadata */
static void
pocl_write_pixel_fast_ui (uint4 color, int4 coord, int width, int height,
                          int depth, int order, int elem_size, void *data)
{
  size_t base_index = coord.x + coord.y * width;
  if (depth)
    base_index += (coord.z * height * width);

  if (coord.x >= width || coord.x < 0 || coord.y >= height || coord.y < 0
      || (depth != 0 && (coord.z >= depth || coord.z < 0)))
    {
      return;
    }

  if (order == CL_A)
    {
      if (elem_size == 1)
        ((uchar *)data)[base_index] = convert_uchar_sat (color.w);
      else if (elem_size == 2)
        ((ushort *)data)[base_index] = convert_ushort_sat (color.w);
      else if (elem_size == 4)
        ((uint *)data)[base_index] = color.w;
      return;
    }

  if (elem_size == 1)
    {
      ((uchar4 *)data)[base_index] = convert_uchar4_sat (color);
    }
  else if (elem_size == 2)
    {
      ((ushort4 *)data)[base_index] = convert_ushort4_sat (color);
    }
  else if (elem_size == 4)
    {
      ((uint4 *)data)[base_index] = color;
    }
  return;
}

/* for use inside filter functions
 * no channel mapping
 * no pointers to img metadata */
static void
pocl_write_pixel_fast_f (float4 color, int4 coord, int width, int height,
                         int depth, int channel_type, int order, void *data)
{
  size_t base_index = coord.x + coord.y * width;
  if (depth)
    base_index += (coord.z * height * width);

  if (coord.x >= width || coord.x < 0 || coord.y >= height || coord.y < 0
      || (depth != 0 && (coord.z >= depth || coord.z < 0)))
    {
      return;
    }

  if (order == CL_A)
    {
      write_float_pixel (color.w, data, base_index, channel_type);
    }
  else
    {
      write_float4_pixel (color, data, base_index, channel_type);
    }

  return;
}

/* for use inside filter functions
 * no channel mapping
 * no pointers to img metadata */
static void
pocl_write_pixel_fast_i (int4 color, int4 coord, int width, int height,
                         int depth, int order, int elem_size, void *data)
{
  size_t base_index = coord.x + coord.y * width;
  if (depth)
    base_index += (coord.z * height * width);

  if (coord.x >= width || coord.x < 0 || coord.y >= height || coord.y < 0
      || (depth != 0 && (coord.z >= depth || coord.z < 0)))
    {
      return;
    }

  if (order == CL_A)
    {
      if (elem_size == 1)
        ((char *)data)[base_index] = convert_char_sat (color.w);
      else if (elem_size == 2)
        ((short *)data)[base_index] = convert_short_sat (color.w);
      else if (elem_size == 4)
        ((int *)data)[base_index] = color.w;
      return;
    }

  if (elem_size == 1)
    {
      ((char4 *)data)[base_index] = convert_char4_sat (color);
    }
  else if (elem_size == 2)
    {
      ((short4 *)data)[base_index] = convert_short4_sat (color);
    }
  else if (elem_size == 4)
    {
      ((int4 *)data)[base_index] = color;
    }
  return;
}

/* full write with channel map conversion etc
 * Writes a four element pixel to an image pixel pointed by integer coords.
 */
static void
pocl_write_pixel (uint4 color, global dev_image_t *img, int4 coord)
{
  int width = img->_width;
  int height = img->_height;
  int depth = img->_depth;
  int order = img->_order;
  int elem_size = img->_elem_size;
  int channel_type = img->_data_type;
  void *data = img->_data;

  color = map_channels (color, order);

  if ((channel_type == CL_SIGNED_INT8) || (channel_type == CL_SIGNED_INT16)
      || (channel_type == CL_SIGNED_INT32))
    pocl_write_pixel_fast_i (as_int4 (color), coord, width, height, depth,
                             order, elem_size, data);
  else if ((channel_type == CL_UNSIGNED_INT8)
           || (channel_type == CL_UNSIGNED_INT16)
           || (channel_type == CL_UNSIGNED_INT32))
    pocl_write_pixel_fast_ui (as_uint4 (color), coord, width, height, depth,
                              order, elem_size, data);
  else // TODO unsupported channel types
    pocl_write_pixel_fast_f (as_float4 (color), coord, width, height, depth,
                             channel_type, order, data);
}

/*
write_imagei can only be used with image objects created with
image_channel_data_type set to one of the following values:
CL_SIGNED_INT8, CL_SIGNED_INT16, and CL_SIGNED_INT32.

write_imageui functions can only be used with image objects created with
image_channel_data_type set to one of the following values:
CL_UNSIGNED_INT8, CL_UNSIGNED_INT16, or CL_UNSIGNED_INT32.
*/

/*
 * write_imagef can only be used with image objects created with
 * image_channel_data_type set to one of the pre-defined packed formats,
 * or set to CL_SNORM_INT8, CL_UNORM_INT8, CL_SNORM_INT16,
 * CL_UNORM_INT16, CL_HALF_FLOAT or CL_FLOAT.
*/

#define IMPLEMENT_WRITE_IMAGE_INT_COORD(__IMGTYPE__, __POSTFIX__, __COORD__,  \
                                        __DTYPE__)                            \
  void _CL_OVERLOADABLE write_image##__POSTFIX__ (                            \
      __IMGTYPE__ image, __COORD__ coord, __DTYPE__ color)                    \
  {                                                                           \
    int4 coord4;                                                              \
    INITCOORD##__COORD__ (coord4, coord);                                     \
    global dev_image_t *i_ptr                                                 \
        = __builtin_astype (image, global dev_image_t *);                     \
    pocl_write_pixel (as_uint4 (color), i_ptr, coord4);                       \
  }

IMPLEMENT_WRITE_IMAGE_INT_COORD ( IMG_WO_AQ image2d_t, ui, int2, uint4)
IMPLEMENT_WRITE_IMAGE_INT_COORD ( IMG_WO_AQ image2d_t, ui, int4, uint4)

IMPLEMENT_WRITE_IMAGE_INT_COORD ( IMG_WO_AQ image2d_t, i, int2, int4)
IMPLEMENT_WRITE_IMAGE_INT_COORD ( IMG_WO_AQ image2d_t, i, int4, int4)

IMPLEMENT_WRITE_IMAGE_INT_COORD (IMG_WO_AQ image2d_t, f, int2, float4)
IMPLEMENT_WRITE_IMAGE_INT_COORD (IMG_WO_AQ image3d_t, f, int4, float4)

#ifdef CLANG_HAS_RW_IMAGES

IMPLEMENT_WRITE_IMAGE_INT_COORD ( IMG_RW_AQ image2d_t, ui, int2, uint4)
IMPLEMENT_WRITE_IMAGE_INT_COORD ( IMG_RW_AQ image2d_t, ui, int4, uint4)

IMPLEMENT_WRITE_IMAGE_INT_COORD ( IMG_RW_AQ image2d_t, i, int2, int4)
IMPLEMENT_WRITE_IMAGE_INT_COORD ( IMG_RW_AQ image2d_t, i, int4, int4)

IMPLEMENT_WRITE_IMAGE_INT_COORD (IMG_RW_AQ image2d_t, f, int2, float4)
IMPLEMENT_WRITE_IMAGE_INT_COORD (IMG_RW_AQ image3d_t, f, int4, float4)

#endif
