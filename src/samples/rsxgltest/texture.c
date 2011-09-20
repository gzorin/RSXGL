#include <assert.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include <png.h>

#include <pngdec/pngdec.h>

#include "texture.h"

void ReadDataFromMemory(png_structp png_ptr, png_bytep outBytes, 
	png_size_t byteCountToRead);
void *seek; // Yeah, this isn't threadsafe, so only load 1 png at a time.

void parseRGB(uint32_t *dest, uint32_t width, uint32_t height,
		png_structp png_ptr, png_infop info_ptr) {

	png_uint_32 bytesPerRow = png_get_rowbytes(png_ptr, info_ptr);
	uint8_t *rowData = malloc(bytesPerRow);

	// Read each row
	int row, col;
	for(row = 0; row < height; row++) {
		png_read_row(png_ptr, (png_bytep) rowData, NULL);

		int index = 0;
		for(col = 0; col < width; col++) {
			uint8_t red   = rowData[index++];
			uint8_t green = rowData[index++];
			uint8_t blue  = rowData[index++];

			//*dest = red << 16 | green << 8 | blue;
			//*dest = 0xff << 24 | blue << 16 | green << 8 | red;
			*dest = blue << 24 | green << 16 | red << 8 | 0xff;
			//*dest = red << 24 | green << 16 | blue << 8 | 0xff;
			dest++;	
		}
	}

	free(rowData);
}

void parseRGBA(uint32_t *dest, uint32_t width, uint32_t height,
		png_structp png_ptr, png_infop info_ptr) {

	png_uint_32 bytesPerRow = png_get_rowbytes(png_ptr, info_ptr);
	uint8_t *rowData = malloc(bytesPerRow);

	// Read each row
	int row, col;
	for(row = 0; row < height; row++) {
		png_read_row(png_ptr, (png_bytep) rowData, NULL);

		int index = 0;
		for(col = 0; col < width; col++) {
			uint8_t red   = rowData[index++];
			uint8_t green = rowData[index++];
			uint8_t blue  = rowData[index++];
			uint8_t alpha = rowData[index++];

			//*dest = alpha << 24 | red << 16 | green << 8 | blue;
			//*dest = alpha << 24 | blue << 16 | green << 8 | red;
			*dest = blue << 24 | green << 16 | red << 8 | alpha;
			//*dest = red << 24 | green << 16 | blue << 8 | alpha;
			dest++;	
		}
	}
	free(rowData);
}

// Load a png from ram 
// I can't be bothered handling errors correctly, lets just abort
Image loadPng(const uint8_t *png) {
	// Make sure we have a valid png here.
	assert(png_sig_cmp((png_bytep) png, 0, 8) == 0);

	// get PNG file info struct
	png_structp png_ptr = NULL;
	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	assert(png_ptr != NULL);

	// get PNG image data info struct
	png_infop info_ptr = NULL;
	info_ptr = png_create_info_struct(png_ptr);
	assert(info_ptr != NULL);

	png_set_read_fn(png_ptr, (png_bytep) png, ReadDataFromMemory);

	// seek to start of png.
	seek = NULL;

	png_read_info(png_ptr, info_ptr);

	png_uint_32 width = 0;
	png_uint_32 height = 0;
	int bitDepth = 0;
	int colorType = -1;
	assert(png_get_IHDR(png_ptr, info_ptr,
		&width,
		&height,
		&bitDepth,
		&colorType,
		NULL, NULL, NULL) == 1);

	Image image;
	image.data = memalign(16, 4 * width * height);
	image.width = width;
	image.height = height;

	switch(colorType) {
	case PNG_COLOR_TYPE_RGB:
		parseRGB(image.data, width, height, png_ptr, info_ptr);
		break;
	case PNG_COLOR_TYPE_RGBA:
		parseRGBA(image.data, width, height, png_ptr, info_ptr);
		break;
	default:
		printf("Unsupported png type\n");
		abort();
	}

	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

	return image;
}

void ReadDataFromMemory(png_structp png_ptr, png_bytep outBytes, 
	png_size_t byteCountToRead) {
	if (seek == NULL) seek = png_get_io_ptr(png_ptr);

	memcpy(outBytes, seek, byteCountToRead);
	seek = seek + byteCountToRead;
}
