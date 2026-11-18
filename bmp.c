/* bmp.c */
#include "include/bmp.h"
#include "include/omnistd.h"

typedef struct s_bmp_palette_entry {
  i8 blue;
  i8 green;
  i8 red;
  i8 reserved;
} packed Bmp_Palette_Entry;

typedef struct s_bmp_stream {
  i16 fd;
  i16 position;
  i16 length;
  i8 buffer[512];
} Bmp_Stream;

/// Bps is always a multiple of 4 or 1
// For [r,g,b,reserved]
static i8 supported_bpp(i16 bpp) { return bpp == 1 || bpp == 4 || bpp == 8 || bpp == 16 || bpp == 24 || bpp == 32; }
static i16 absolute_height(s32 height) { return (i16)(height < 0 ? -height : height); }

// Pulls bytes from a Bmp_stream until it retunrs 0(All bytes processed)
static i8 stream_byte(Bmp_Stream *stream, i8 *value) {
  if (stream->position >= stream->length) {
    stream->length = x_read(stream->fd, stream->buffer, sizeof(stream->buffer));
    stream->position = 0;
    if (!stream->length || stream->length == 0xffff) return 0;
  }
  *value = stream->buffer[stream->position++];
  return 1;
}

/// Reads all the palettes
static i8 read_palette(i16 fd, Bitmap *bmp, i16 count) {
  Bmp_Palette_Entry entry;
  if (!count) return 1;
  bmp->palette = (RGB *)alloc((i16)(count * sizeof(RGB)));
  if (!bmp->palette) return 0;
  bmp->n_palette = count;
  for (i16 n = 0; n < count; n++) {
    if (!readn(fd, &entry, sizeof(entry))) return 0;
    bmp->palette[n].red = entry.red;
    bmp->palette[n].green = entry.green;
    bmp->palette[n].blue = entry.blue;
    bmp->palette[n]._ = 0;
  }
  return 1;
}

Bitmap *parse_bmp(i8 *image, i16 x, i16 y) {
  i16 n_palette = 0;
  if (!image) return 0;
  save();
  i8 * fname = addbmp(image);
  if (!fname) goto failed;
  i16 fd = open(fname, 0);
  if (!fd) goto failed;
  Bmp_header *hdr = (Bmp_header *)alloc(sizeof(Bmp_header));
  Bmp_Info_header *info = (Bmp_Info_header *)alloc(sizeof(Bmp_Info_header));
  Bitmap *bmp = (Bitmap *)alloc(sizeof(Bitmap));
  if (!hdr || !info || !bmp) goto failed;
  
  /// BMP file has base header then info header then data
  if (!readn(fd, hdr, sizeof(Bmp_header)) || !readn(fd, info, sizeof(Bmp_Info_header))) goto failed;

  // Validate signature,header size,width and height,channels, bps, and compress fields
  // Required AI to craft this long ahh check 
  if (hdr->sign != 0x4d42 || info->header_size < sizeof(Bmp_Info_header) || info->header_size > 4096 || info->width <= 0 || info->width > 32767 || info->height == 0 || info->height == (-2147483647 - 1) || absolute_height(info->height) > 32767 || info->planes != 1 || !supported_bpp(info->bps) || info->compress != 0) goto failed;
  if (info->bps <= 8) {
    i16 max_colors = (i16)(1 << info->bps);
    n_palette = info->colors_used ? (i16)info->colors_used : max_colors;
    if (info->colors_used > max_colors || n_palette <= 0 || n_palette > max_colors) goto failed;
  }
  i32 palette_offset = 14 + info->header_size;
  i32 palette_end = palette_offset + ((i32)n_palette * 4);
  if (hdr->offset < palette_end || (hdr->file_size && (hdr->offset > hdr->file_size || palette_end > hdr->file_size))) goto failed;
  bmp->fname = fname;
  bmp->x = x;
  bmp->y = y;
  bmp->header = hdr;
  bmp->info = info;
  bmp->palette = 0;
  bmp->n_palette = 0;
  if (n_palette) { if (!seek(fd, palette_offset) || !read_palette(fd, bmp, n_palette)) goto failed; }
  if (hdr->file_size) {
    i32 row_bits = (info->width * info->bps + 31) & ~31;
    i32 row_stride = row_bits >> 3;
    i32 image_bytes = row_stride * absolute_height(info->height);
    if (hdr->offset > hdr->file_size || image_bytes > hdr->file_size - hdr->offset) goto failed;
  }
  close(fd);
  return bmp;

failed:
  if (fd) close(fd);
  load();
  return 0;
}

void bmp_show(Bitmap *bmp) {
  if (!bmp) return;
  printf("Name: %s\nX: %s\nY: %s\nWidth: %s\nHeight: %s\nBPP: %s\n", bmp->fname,tostr(bmp->x), tostr(bmp->y), tostr((i16)bmp->info->width),tostr(absolute_height(bmp->info->height)), tostr(bmp->info->bps));
}

i8 *addbmp(i8 *name) {
  i16 len = 0;
  if (!name) return 0;
  while (name[len]) len++;
  i8 *filename = (i8 *)alloc((i16)(len + 5));
  if (!filename) return 0;
  copy_(filename, name, len);
  filename[len++] = '.';
  filename[len++] = 'b';
  filename[len++] = 'm';
  filename[len++] = 'p';
  filename[len] = 0;
  return filename;
}

/// Finds the nearest vga color to the pixel 
static i8 nearest_vga_color(i8 red, i8 green, i8 blue) {
  static const i8 vga[16][3] = {{0, 0, 0},     {0, 0, 170},    {0, 170, 0},    {0, 170, 170},{170, 0, 0},   {170, 0, 170},  {170, 85, 0},   {170, 170, 170},{85, 85, 85},  {85, 85, 255},  {85, 255, 85},  {85, 255, 255},{255, 85, 85}, {255, 85, 255}, {255, 255, 85}, {255, 255, 255}};
  s32 best_distance = 0xffffffff;
  i8 best = 0;
  i16 n;
  for (n = 0; n < 16; n++) {
    s32 dr = (s32)red - vga[n][0];
    s32 dg = (s32)green - vga[n][1];
    s32 db = (s32)blue - vga[n][2];
    s32 distance = dr * dr + dg * dg + db * db;
    if (distance < best_distance) {
      best_distance = distance;
      best = (i8)n;
    }
  }
  return best;
}
// Draw pixel from bmp
static i8 draw_indexed_pixel(Bitmap *bmp, i32 col, i32 row, i16 index) {
  i32 screen_x = (i32)bmp->x + col;
  i32 screen_y = (i32)bmp->y + row;
  Point point;
  if (index >= bmp->n_palette) return 0;
  if (screen_x < 0 || screen_x >= MAX_X || screen_y < 0 || screen_y >= MAX_Y) return 1;
  RGB *color = &bmp->palette[index];
  point.x = (i16)screen_x;
  point.y = (i16)screen_y;
  point.color = nearest_vga_color(color->red, color->green, color->blue);
  draw_point(&point);
  return 1;
}

// Draw pixel using specific r,g,b
static void draw_rgb_pixel(Bitmap *bmp, i32 col, i32 row, i8 red, i8 green, i8 blue) {
  i32 screen_x = (i32)bmp->x + col;
  i32 screen_y = (i32)bmp->y + row;
  Point point;
  if (screen_x < 0 || screen_x >= MAX_X || screen_y < 0 || screen_y >= MAX_Y) return;
  point.x = (i16)screen_x;
  point.y = (i16)screen_y;
  point.color = nearest_vga_color(red, green, blue);
  draw_point(&point);
}

i8 draw_bmp(Bitmap *bmp) {
  if (!bmp || !bmp->info || !videoflag) return 0;
  i8 ok = 1;
  i8 byte, b, g, r, alpha;
  Bmp_Stream stream;
  i16 width = (i16)bmp->info->width,height = absolute_height(bmp->info->height);
  i16 bpp = bmp->info->bps;
  i32 row_bytes = ((i32)width * bpp + 7) >> 3,row_stride = (((i32)width * bpp + 31) & ~31) >> 3;
  i16 fd = open(bmp->fname, bmp->header->offset);
  if (!fd) return 0;
  stream.fd = fd;
  stream.position = 0;
  stream.length = 0;
  for (i32 row = 0; row < height && ok; row++) {
    i32 output_row = bmp->info->height > 0 ? height - 1 - row : row;
    i32 col = 0;
    if (bpp == 1 || bpp == 4) {
      while (col < width) {
        i16 pixels_per_byte = bpp == 1 ? 8 : 2;
        if (!stream_byte(&stream, &byte)) {
          ok = 0;
          break;
        }
        for (i16 p = 0; p < pixels_per_byte && col < width; p++, col++) {
          i16 index;
          if (bpp == 1) index = (byte >> (7 - p)) & 1;
          else index = p == 0 ? (byte >> 4) & 0x0f : byte & 0x0f;
          if (!draw_indexed_pixel(bmp, col, output_row, index)) {
            ok = 0;
            break;
          }
        }
      }
    } else if (bpp == 8) {
      for (col = 0; col < width; col++) {
        if (!stream_byte(&stream, &byte) || !draw_indexed_pixel(bmp, col, output_row, byte)) {
          ok = 0;
          break;
        }
      }
    } else if (bpp == 16) {
      for (col = 0; col < width; col++) {
        if (!stream_byte(&stream, &byte)) {
          ok = 0;
          break;
        }
        i16 pixel = byte;
        if (!stream_byte(&stream, &byte)) {
          ok = 0;
          break;
        }
        pixel |= (i16)byte << 8;
        // 5:5:5 RGB layout
        i8 blue = (i8)(((pixel & 0x001f) << 3) | ((pixel & 0x001f) >> 2));
        i8 green = (i8)((((pixel >> 5) & 0x001f) << 3) | (((pixel >> 5) & 0x001f) >> 2));
        i8 red = (i8)((((pixel >> 10) & 0x001f) << 3) | (((pixel >> 10) & 0x001f) >> 2));
        draw_rgb_pixel(bmp, col, output_row, red, green, blue);
      }
    } else {
      for (col = 0; col < width; col++) {
        if (!stream_byte(&stream, &b) || !stream_byte(&stream, &g) || !stream_byte(&stream, &r)) {
          ok = 0;
          break;
        }
        if (bpp == 32 && !stream_byte(&stream, &alpha)) {
          ok = 0;
          break;
        }
        draw_rgb_pixel(bmp, col, output_row, r, g, b);
      }
    }

    /* Pixel rows are padded to a four-byte boundary. */
    for (col = row_bytes; col < row_stride && ok; col++)
      if (!stream_byte(&stream, &byte))
        ok = 0;
  }
  close(fd);
  return ok;
}
