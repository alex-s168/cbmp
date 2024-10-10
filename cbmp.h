#ifndef CBMP_CBMP_H
#define CBMP_CBMP_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BMP_Pixel BMP_Pixel;

typedef struct BMP_data
{
    unsigned int width;
    unsigned int height;
    unsigned int depth;

// impl
    unsigned int _file_byte_number;
    unsigned char* _file_byte_contents;
    unsigned int _pixel_array_start;
    /** use begin_bmp_as_arr() instead ! */
    BMP_Pixel* _pixels;
} BMP;

typedef enum {
    B_ROW_MAJOR
} BMP_MatrixFormat;

typedef enum {
    B_RGBA8,
    B_RGB8,
} BMP_PixelFormat;

// Public function declarations

BMP* bmp_open(const char* file_path);
BMP* bmp_deep_copy(BMP const* to_copy);
void bmp_get_pixel_rgb(BMP const* bmp, int x, int y, unsigned char* r, unsigned char* g, unsigned char* b);
void bmp_set_pixel_rgb(BMP const* bmp, int x, int y, unsigned char r, unsigned char g, unsigned char b);
unsigned char * bmp_begin_as_arr(BMP const* bmp, BMP_MatrixFormat mf, BMP_PixelFormat pf);
void bmp_end_as_arr(unsigned char *, BMP const*);
void bmp_write(BMP* bmp, char* file_name);
void bmp_close(BMP* bmp);

#ifdef __cplusplus
}
#endif

#endif // CBMP_CBMP_H
