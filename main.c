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
int optIndex = (-1);
int optNoise = 0;
int optTransCol = (-1);

int optDrcs = (-1);
char optDscs[4] = { 0, 0, 0, 0 };
int optDcss = 0;
int optDcn = 0;
int optFontW = 10;
int optFontH = 20;

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
void setUcDscs(long uc)
{
    int n;
    int I = 0, X = 0, F = 0, C = 0;

    if ( optDrcs == (-1) )
	return;

    switch(optDrcs % 10) {
    case 0:
	//	3/0		2/0 3/0		2/0 2/0 3/0
	// 94	-u 0000000	-u 0001D60	-u 001EDDE	01EEFDE
	// 96	-u 1000000	-u 1001E00	-u 101F860	11F9860
	if ( uc < 0 ) {
	    uc = 1345 * 94;
	    optDcss = 0;
	    n = 94;
	} else if ( uc >= 0x1000000 ) {
	    uc -= 0x1000000;
	    optDcss = 1;
	    n = 96;
	} else {
	    optDcss = 0;
	    n = 94;
	}
	if ( uc < (80 * n) ) {
	    // I=0,X=0,F=30-7E		0-79
	    I = X = 0;
	    F = (uc / n) + 0x30;
	} else if ( uc < (1345 * n) ) {
	    // I=0,X=20-2F,F=30-7E	80+16*79=1344
	    uc -= (80 * n);
	    I = 0;
	    X = (uc / n) / 79 % 16 + 0x20;
	    F = (uc / n) % 79 + 0x30;
	} else {
	    // I=20-2F,X=20-2F,F=30-7E	1345+16*16*79=21569
	    uc -= (1345 * n);
	    I = (uc / n) / 79 / 16 + 0x20;
	    X = (uc / n) / 79 % 16 + 0x20;
	    F = (uc / n) % 79 + 0x30;
	}
	C = uc % n + (optDcss == 0 ? 0x21 : 0x20);
	break;
    case 1:	// DRCSMMv2
	if ( uc < 0 )
	    uc = 0;
	else if ( uc >= 0x100000 )
	    uc -= 0x100000;
	I = 0;
	X = 0x20;
	if ( (F = (uc >> 8) & 0xFF) < 0x30 ) F = 0x30;
	if ( (C = uc & 0x7F) < 0x20 ) C = 0x20;
	optDcss = (uc & 0x80) == 0 ? 0 : 1;
	break;
    case 2:	// DRCSMMv3
	if ( uc < 0 )
	    uc = 0;
	else if ( uc >= 0x100000 )
	    uc -= 0x100000;
	I = 0x20;
	X = (uc / 94) / 63 + 0x20;
	F = (uc / 94) % 63 + 0x40;
	C = (uc % 94) + 0x21;
	optDcss = 0;
	break;
    }

    n = 0;
    if ( I >= 0x20 && I <= 0x2F )
	optDscs[n++] = (char)I;
    if ( X >= 0x20 && X <= 0x2F )
	optDscs[n++] = (char)X;
    if ( F >= 0x30 && F <= 0x7E )
	optDscs[n++] = (char)F;
    optDscs[n] = '\0';

    if ( C >= 0x20 && C <= 0x7F )
    	optDcn = C - 0x20;
}
long htol(char *p)
{
    long l = 0;

    while ( *p != '\0' ) {
	if ( *p >= '0' && *p <= '9' )
	    l = l * 16 + (*(p++) - '0');
	else if ( *p >= 'A' && *p <= 'F' )
	    l = l * 16 + (*(p++) - 'A' + 10);
	else if ( *p >= 'a' && *p <= 'f' )
	    l = l * 16 + (*(p++) - 'a' + 10);
	else
	    break;
    }
    return l;
}
void usage(char *name)
{
    fprintf(stderr, "Usage: %s [-p MaxPalet] [-b ColorBits] [-m MaxValue...] [-s ColorHex] [-tf]\n"\
 		    "\t[-d DRCS (0/1/2)|(10/11/12)|(20/21/23)] [-u 100000-10FFFF] [-x FontWidth]\n"\
		    "\t[-i number] [-n noise (0-100)] [-w width] [-h height] <file name...>\n", name);
}
int main(int ac, char *av[])
{
    int n;
    int mx = 1;
    long uc = (-1);

    while ( (n = getopt(ac, av, "p:w:h:b:m:tfi:n:d:u:x:s:")) != EOF ) {
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
	case 'i':
	    optIndex = atoi(optarg);
	case 'n':
	    optNoise = atoi(optarg);
	    break;
	case 'd':
	    optDrcs = atoi(optarg);
	    break;
	case 'u':
	    uc = htol(optarg);
	    break;
	case 'x':
	    if ( (optFontW = atoi(optarg)) < 5 )
		optFontW = 5;
	    optFontH = optFontW * 2;
	    break;
	case 's':
	    optTransCol = (int)htol(optarg);
	    break;
	default:
	    usage(av[0]);
	    exit(0);
	}
    }

    setUcDscs(uc);

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
