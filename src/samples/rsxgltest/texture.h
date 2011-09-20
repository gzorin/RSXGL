#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void load_tex(uint32_t unit, uint32_t offset, uint32_t width, uint32_t height, uint32_t stride, uint32_t fmt, int smooth );

typedef struct {
	uint32_t *data;
	uint32_t width;
	uint32_t height;
} Image;

Image loadPng(const uint8_t *png);

#ifdef __cplusplus
}
#endif
