/* A small PNG writer for the host tests: 8 bit RGB, stored (uncompressed)
   deflate blocks. Returns 1 on success. */
#ifndef HOST_PNG_H
#define HOST_PNG_H
int png_write_rgb(const char *path, const unsigned char *rgb, int width, int height);
#endif
