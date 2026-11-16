/* bmp.h */
#pragma once
#include "gui.h"
#include "shapes.h"

typedef unsigned char i8;
typedef unsigned short i16;
typedef unsigned int i32;
typedef signed int s32;
typedef unsigned long i64;

#define public __attribute__((visibility("default")))
#define internal __attribute__((visibility("hidden")))
#define private static
#define packed __attribute__((packed))
struct s_bmp {
    i16 sign;
    i32 file_size;
    i16 res1;
    i16 res2;
    i32 offset;
}packed;

typedef struct s_bmp Bmp_header;

struct s_bmp_info{
    i32 header_size;
    s32 width;
    s32 height;
    i16 planes;
    i16 bps;
    i32 compress;
    i32 image_size;
    s32 x_pixels;
    s32 y_pixels;
    i32 colors_used;
    i32 colors_imp;
} packed;

typedef struct s_bmp_info Bmp_Info_header;

struct s_bitmap{
    i8 *fname;
    i16 x,y;
    Bmp_header *header;
    Bmp_Info_header *info;
    RGB *palette;
    i16 n_palette;
} packed;

typedef struct s_bitmap Bitmap;

Bitmap * parse_bmp(i8*,i16,i16);
i8 draw_bmp(Bitmap*);
i8* addbmp(i8*);
void bmp_show(Bitmap*);