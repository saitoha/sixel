#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <gd.h>

typedef unsigned char BYTE;

void gdImageSixel(gdImagePtr gd, FILE *out);
gdImagePtr gdImageCreateFromSixelPtr(int len, BYTE *p);
gdImagePtr gdImageCreateFromPnmPtr(int len, BYTE *p);

#define	FMT_GIF	    0
#define	FMT_PNG	    1
#define	FMT_BMP	    2
#define	FMT_JPG	    3
#define	FMT_TGA	    4
#define	FMT_WBMP    5
#define	FMT_TIFF    6
#define	FMT_SIXEL   7
#define	FMT_PNM	    8
#define	FMT_GD2     9

int maxPalet = gdMaxColors;
int maxValue[4] = { 100, 100, 100, 100 };
int optTrue = 0;
int optFill = 0;
int resWidth = (-1);
int resHeight = (-1);

static int FileFmt(int len, BYTE *data)
{
    if ( len < 3 )
	return (-1);

    if ( len > 18 && memcmp("TRUEVISION", data + len - 18, 10) == 0 )
	return FMT_TGA;

    if ( memcmp("GIF", data, 3) == 0 )
	return FMT_GIF;

    if ( len > 8 && memcmp("\x89\x50\x4E\x47\x0D\x0A\x1A\x0A", data, 8) == 0 )
	return FMT_PNG;

    if ( memcmp("BM", data, 2) == 0 )
	return FMT_BMP;

    if ( memcmp("\xFF\xD8", data, 2) == 0 )
	return FMT_JPG;

    if ( memcmp("\x00\x00", data, 2) == 0 )
	return FMT_WBMP;

    if ( memcmp("\x4D\x4D", data, 2) == 0 )
	return FMT_TIFF;

    if ( memcmp("\x49\x49", data, 2) == 0 )
	return FMT_TIFF;

    if ( memcmp("\033P", data, 2) == 0 )
	return FMT_SIXEL;

    if ( data[0] == 0x90  && (data[len-1] == 0x9C || data[len-2] == 0x9C) )
	return FMT_SIXEL;

    if ( data[0] == 'P' && data[1] >= '1' && data[1] <= '6' )
	return FMT_PNM;

    if ( memcmp("gd2", data, 3) == 0 )
	return FMT_GD2;

    return (-1);
}

static gdImagePtr LoadFile(char *filename)
{
    int n, len, max;
    FILE *fp = stdin;
    BYTE *data;
    gdImagePtr im = NULL;

    if ( filename != NULL && (fp = fopen(filename, "r")) == NULL )
	return NULL;

    len = 0;
    max = 64 * 1024;

    if ( (data = (BYTE *)malloc(max)) == NULL )
	return NULL;

    for ( ; ; ) {
	if ( (max - len) < 4096 ) {
	    max *= 2;
    	    if ( (data = (BYTE *)realloc(data, max)) == NULL )
		return NULL;
	}
	if ( (n = fread(data + len, 1, 4096, fp)) <= 0 )
	    break;
	len += n;
    }

    if ( fp != stdout )
    	fclose(fp);

    switch(FileFmt(len, data)) {
	case FMT_GIF:
	    im = gdImageCreateFromGifPtr(len, data);
	    break;
	case FMT_PNG:
	    im = gdImageCreateFromPngPtr(len, data);
	    break;
	case FMT_BMP:
	    im = gdImageCreateFromBmpPtr(len, data);
	    break;
	case FMT_JPG:
	    im = gdImageCreateFromJpegPtrEx(len, data, 1);
	    break;
	case FMT_TGA:
	    im = gdImageCreateFromTgaPtr(len, data);
	    break;
	case FMT_WBMP:
	    im = gdImageCreateFromWBMPPtr(len, data);
	    break;
	case FMT_TIFF:
	    im = gdImageCreateFromTiffPtr(len, data);
	    break;
	case FMT_SIXEL:
	    im = gdImageCreateFromSixelPtr(len, data);
	    break;
	case FMT_PNM:
	    im = gdImageCreateFromPnmPtr(len, data);
	    break;
	case FMT_GD2:
	    im = gdImageCreateFromGd2Ptr(len, data);
	    break;
    }

    free(data);

    return im;
}

static int ConvSixel(char *filename)
{
    gdImagePtr im = NULL;
    gdImagePtr dm = NULL;
    int bReSize;

    bReSize = (resWidth > 0 || resHeight > 0 ? 1 : 0);

    if ( (im = LoadFile(filename)) == NULL )
	return (-1);

    if ( bReSize ) {
	if ( resWidth <= 0 )
	    resWidth = resHeight * gdImageSX(im) / gdImageSY(im);
	if ( resHeight <= 0 )
	    resHeight = resWidth * gdImageSY(im) / gdImageSX(im);

    	if ( (dm = gdImageCreateTrueColor(resWidth, resHeight)) == NULL )
	    return 1;

	gdImageCopyResampled(dm, im, 0, 0, 0, 0, 
		resWidth, resHeight, gdImageSX(im), gdImageSY(im));
    	gdImageDestroy(im);
	im = dm;
    }

    gdImageSixel(im, stdout);
    gdImageDestroy(im);

    return 0;
}
void setBitsMax(int bits)
{
    if ( bits < 1 || bits > 16 )
	return;

    maxValue[0] = maxValue[1] = maxValue[2] = maxValue[3] = (1 << bits) - 1;
}
void setMaxValue(char *str)
{
    int n = 3;

    while ( n >= 0 && *str != '\0' ) {
	maxValue[n--] = atoi(str);

	while ( isdigit(*str) )
	    str++;
	while ( *str != '\0' && !isdigit(*str) )
	    str++;
    }

    for ( ; n >= 0 && n < 3 ; n-- )
	maxValue[n] = maxValue[n + 1];
}
void usage(char *name)
{
    fprintf(stderr, "Usage: %s [-p MaxPalet] [-b ColorBits] [-m MaxValue...] [-tf] "\
		    "[-w width] [-h height] <file name...>\n", name);
}
int main(int ac, char *av[])
{
    int n;
    int mx = 1;

    while ( (n = getopt(ac, av, "p:w:h:b:m:tf")) != EOF ) {
	switch(n) {
	case 'p':
	    maxPalet = atoi(optarg);
	    break;
	case 'w':
	    resWidth = atoi(optarg);
	    break;
	case 'h':
	    resHeight = atoi(optarg);
	    break;
	case 'b':
	    setBitsMax(atoi(optarg));
	    break;
	case 'm':
	    setMaxValue(optarg);
	    break;
	case 't':
	    optTrue = 1;
	    break;
	case 'f':
	    optFill = 1;
	    break;
	default:
	    usage(av[0]);
	    exit(0);
	}
    }

    while ( optind < ac )
	av[mx++] = av[optind++];

    if ( mx <= 1 ) {
	ConvSixel(NULL);

    } else {
    	for ( n = 1 ; n < mx ; n++ )
	    ConvSixel(av[n]);
    }

    return 0;
}
