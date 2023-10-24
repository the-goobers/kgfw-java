#include "ktga.h"
#include <stdlib.h>
#include <string.h>

#define U8(buf, i) *(((unsigned char *) buf) + i)
#define U16(buf, i) *(((unsigned char *) buf) + i) | (*(((unsigned char *) buf) + i + 1) << 8)

int ktga_load(ktga_t * out_tga, void * buffer, unsigned long long int buffer_length) {
	if (buffer_length <= 18 || buffer == NULL) {
		return 1;
	}

	unsigned char * buf = (unsigned char *) buffer;
	out_tga->header.id_len = U8(buf, 0);
	out_tga->header.color_map_type = U8(buf, 1);
	out_tga->header.img_type = U8(buf, 2);
	out_tga->header.color_map_origin = U16(buf, 3);
	out_tga->header.color_map_length = U16(buf, 5);
	out_tga->header.color_map_depth = U8(buf, 7);
	out_tga->header.img_x_origin = U16(buf, 8);
	out_tga->header.img_y_origin = U16(buf, 10);
	out_tga->header.img_w = U16(buf, 12);
	out_tga->header.img_h = U16(buf, 14);
	out_tga->header.bpp = U8(buf, 16);
	out_tga->header.img_desc = U8(buf, 17);
	out_tga->bitmap = malloc(out_tga->header.img_w * out_tga->header.img_h * (out_tga->header.bpp / 8));
	if (out_tga->bitmap == NULL) {
		return 2;
	}

	switch (out_tga->header.img_type) {
		case 1:;
			unsigned char * map = &buf[18];
			for (unsigned long long int i = 0; i < out_tga->header.img_w * out_tga->header.img_h; ++i) {
				memcpy(&out_tga->bitmap[i * out_tga->header.bpp / 8], &map[buf[18 + out_tga->header.color_map_length]], out_tga->header.color_map_depth / 8);
			}
		case 2:
			memcpy(out_tga->bitmap, &buf[18], out_tga->header.img_w * out_tga->header.img_h * (out_tga->header.bpp / 8));
			break;
	}

	return 0;
}

void ktga_destroy(ktga_t * tga) {
	free(tga->bitmap);
}