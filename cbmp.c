#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include "cbmp.h"

// Constants

#define BITS_PER_BYTE 8

#define BLUE 0
#define GREEN 1
#define RED 2
#define ALPHA 3

#define PIXEL_ARRAY_START_BYTES 4
#define PIXEL_ARRAY_START_OFFSET 10

#define WIDTH_BYTES 4
#define WIDTH_OFFSET 18

#define HEIGHT_BYTES 4
#define HEIGHT_OFFSET 22

#define DEPTH_BYTES 2
#define DEPTH_OFFSET 28

struct BMP_Pixel {
    unsigned char red;
    unsigned char green;
    unsigned char blue;
    unsigned char alpha;
} __attribute__ ((packed));

// Private function declarations

static void _throw_error(char* message);
static unsigned int _get_int_from_buffer(unsigned int bytes, 
                                         unsigned int offset, 
                                         unsigned char* buffer);
static unsigned int _get_file_byte_number(FILE* fp);
static unsigned char* _get_file_byte_contents(FILE* fp, unsigned int file_byte_number);
static int _validate_file_type(unsigned char* file_byte_contents);
static int _validate_depth(unsigned int depth);
static unsigned int _get_pixel_array_start(unsigned char* file_byte_contents);
static int _get_width(unsigned char* file_byte_contents);
static int _get_height(unsigned char* file_byte_contents);
static unsigned int _get_depth(unsigned char* file_byte_contents);
static void _update_file_byte_contents(BMP* bmp, int index, int offset, int channel);
static void _populate_pixel_array(BMP* bmp);
static void _map(BMP* bmp, void (*f)(BMP* bmp, int, int, int));
static void _get_pixel(BMP* bmp, int index, int offset, int channel);

// Public function implementations

BMP* bmp_open(const char* file_path)
{
    FILE* fp = fopen(file_path, "rb");
  
    if (fp == NULL)
        return NULL;

    BMP* bmp = (BMP*) malloc(sizeof(BMP));
    if (bmp == NULL) {
        fclose(fp);
        return NULL;
    }

    bmp->_file_byte_number = _get_file_byte_number(fp);
    bmp->_file_byte_contents = _get_file_byte_contents(fp, bmp->_file_byte_number);
    fclose(fp);
    if (bmp->_file_byte_contents == NULL) {
        free(bmp);
        return NULL;
    }

    if(!_validate_file_type(bmp->_file_byte_contents)) {
        free(bmp->_file_byte_contents);
        free(bmp);
        _throw_error("Invalid file type");
    }

    bmp->_pixel_array_start = _get_pixel_array_start(bmp->_file_byte_contents);

    bmp->width = _get_width(bmp->_file_byte_contents);
    bmp->height = _get_height(bmp->_file_byte_contents);
    bmp->depth = _get_depth(bmp->_file_byte_contents);

    if(!_validate_depth(bmp->depth)) {
        free(bmp->_file_byte_contents);
        free(bmp);
        _throw_error("Invalid file depth");
    }

    _populate_pixel_array(bmp);

    return bmp;
}

BMP* bmp_deep_copy(BMP const* to_copy)
{
    BMP* copy = (BMP*) malloc(sizeof(BMP));
    if (copy == NULL) return NULL;

    copy->_file_byte_number = to_copy->_file_byte_number;
    copy->_pixel_array_start = to_copy->_pixel_array_start;
    copy->width = to_copy->width;
    copy->height = to_copy->height;
    copy->depth = to_copy->depth;

    copy->_file_byte_contents = (unsigned char*) malloc(copy->_file_byte_number * sizeof(unsigned char));
    if (copy->_file_byte_contents == NULL) {
        free(copy);
        return NULL;
    }

    unsigned int i;
    for (i = 0; i < copy->_file_byte_number; i++)
    {
        copy->_file_byte_contents[i] = to_copy->_file_byte_contents[i];
    }

    copy->_pixels = (BMP_Pixel*) malloc(copy->width * copy->height * sizeof(BMP_Pixel));
    if (copy->_pixels == NULL) {
        free(copy->_file_byte_contents);
        free(copy);
        return NULL;
    }

    memcpy(copy->_pixels, to_copy->_pixels, sizeof(BMP_Pixel) * (copy->width * copy->height));

    return copy;
}

void bmp_get_pixel_rgb(BMP const* bmp, int x, int y, unsigned char* r, unsigned char* g, unsigned char* b)
{
    int index = y * bmp->width + x;
    *r = bmp->_pixels[index].red;
    *g = bmp->_pixels[index].green;
    *b = bmp->_pixels[index].blue;
}

void bmp_set_pixel_rgb(BMP const* bmp, int x, int y, unsigned char r, unsigned char g, unsigned char b)
{
    int index = y * bmp->width + x;
    bmp->_pixels[index].red = r;
    bmp->_pixels[index].green = g;
    bmp->_pixels[index].blue = b;
}

unsigned char * bmp_begin_as_arr(BMP const* bmp, BMP_MatrixFormat mf, BMP_PixelFormat pf)
{
    switch (pf)
    {
        case B_RGBA8:
            return (unsigned char *) bmp->_pixels;

        case B_RGB8:
        {
            struct RGB {
                unsigned char red;
                unsigned char green;
                unsigned char blue;
            } __attribute__ ((packed));

            unsigned wh = bmp->width * bmp->height;

            struct RGB *buf = malloc(wh * sizeof(struct RGB));
            if (buf == NULL)
                return NULL;

            for (unsigned i = 0; i < wh; i ++)
            {
                BMP_Pixel* p = &bmp->_pixels[i];
                buf[i].red = p->red;
                buf[i].green = p->green;
                buf[i].blue = p->blue;
            }

            return (unsigned char *) buf;
        }
    }
}

void bmp_end_as_arr(unsigned char * arr, BMP const* bmp)
{
    if (arr != (unsigned char *) bmp->_pixels)
    {
        free(arr);
    }
}

void bmp_write(BMP* bmp, char* file_name)
{
    _map(bmp, _update_file_byte_contents);

    FILE* fp = fopen(file_name, "wb");
    if (fp == NULL) {
        _throw_error("could not open file");
    }

    fwrite(bmp->_file_byte_contents, sizeof(char), bmp->_file_byte_number, fp);
    fclose(fp);
}

void bmp_close(BMP* bmp)
{
    free(bmp->_pixels);
    free(bmp->_file_byte_contents);
    free(bmp);
    bmp->_pixels = NULL;
    bmp->_file_byte_contents = NULL;
}


// Private function implementations

static void _throw_error(char* message)
{
    fprintf(stderr, "%s\n", message);
    exit(1);
}

static unsigned int _get_int_from_buffer(unsigned int bytes, 
                                         unsigned int offset, 
                                         unsigned char* buffer)
{
    unsigned int value = 0;
    int i;

    for (i = bytes - 1; i >= 0; --i)
    {
        value <<= 8;
        value += buffer[i + offset];
    }

    return value;
}

static unsigned int _get_file_byte_number(FILE* fp)
{
    unsigned int byte_number;
    fseek(fp, 0, SEEK_END);
    byte_number = ftell(fp);
    rewind(fp);
    return byte_number;
}

static unsigned char* _get_file_byte_contents(FILE* fp, unsigned int file_byte_number)
{
    unsigned char* buffer = (unsigned char*) malloc(file_byte_number * sizeof(char));
    if (buffer == NULL) return NULL; 

    unsigned int result = fread(buffer, 1, file_byte_number, fp);

    if (result != file_byte_number)
    {
        _throw_error("There was a problem reading the file");
    }


    return buffer;
}

static int _validate_file_type(unsigned char* file_byte_contents)
{
    return file_byte_contents[0] == 'B' && file_byte_contents[1] == 'M';
}

static int _validate_depth(unsigned int depth)
{
    return depth == 24 || depth == 32;
}

static unsigned int _get_pixel_array_start(unsigned char* file_byte_contents)
{
    return _get_int_from_buffer(PIXEL_ARRAY_START_BYTES, PIXEL_ARRAY_START_OFFSET, file_byte_contents);
}

static int _get_width(unsigned char* file_byte_contents)
{
    return (int) _get_int_from_buffer(WIDTH_BYTES, WIDTH_OFFSET, file_byte_contents);
}

static int _get_height(unsigned char* file_byte_contents)
{
    return (int) _get_int_from_buffer(HEIGHT_BYTES, HEIGHT_OFFSET, file_byte_contents);
}

static unsigned int _get_depth(unsigned char* file_byte_contents)
{
    return _get_int_from_buffer(DEPTH_BYTES, DEPTH_OFFSET, file_byte_contents);
}

static size_t channel2offset[] = {
    [RED]   = offsetof(struct BMP_Pixel, red),
    [GREEN] = offsetof(struct BMP_Pixel, green),
    [BLUE]  = offsetof(struct BMP_Pixel, blue),
    [ALPHA] = offsetof(struct BMP_Pixel, alpha),
};

static void _update_file_byte_contents(BMP* bmp, int index, int offset, int channel)
{
    char value = *(((unsigned char *) &bmp->_pixels[index]) + channel2offset[channel]);
    bmp->_file_byte_contents[offset + channel] = value;
}

static void _populate_pixel_array(BMP* bmp)
{
    bmp->_pixels = (BMP_Pixel*) malloc(bmp->width * bmp->height * sizeof(BMP_Pixel));
    if (bmp->_pixels == NULL) return;
    _map(bmp, _get_pixel);
}

static void _map(BMP* bmp, void (*f)(BMP*, int, int, int))
{
    int channels = bmp->depth / (sizeof(unsigned char) * BITS_PER_BYTE);
    int row_size = ((int) (bmp->depth * bmp->width + 31) / 32) * 4;
    int padding = row_size - bmp->width * channels;

    int c;
    unsigned int x, y, index, offset;
    for (y = 0; y < bmp->height; y++)
    {
        for (x = 0; x < bmp->width; x++)
        {
            index = y * bmp->width + x;
            offset = bmp->_pixel_array_start + index * channels + y * padding;
            for (c = 0; c < channels; c++)
            {
                (*f)(bmp, index, offset, c);
            }
        }
    }
}

static void _get_pixel(BMP* bmp, int index, int offset, int channel)
{
    unsigned char value = _get_int_from_buffer(sizeof(unsigned char), offset + channel, bmp->_file_byte_contents);
    *(((unsigned char *) &bmp->_pixels[index]) + channel2offset[channel]) = value;
}
