#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <gd.h>

#define PALMAX		1024
#define HASHMAX 	32

#define	PALVAL(n,a,m)	(((n) * (a) + ((m) / 2)) / (m))

typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef __uint32_t DCHAR;

typedef struct _PalNode {
	struct _PalNode *next;
	int	idx;
	int	init;
	WORD	rgba[4];
} PalNode;

typedef struct _SixNode {
	struct _SixNode *next;
	int	pal;
	int	sx;
	int	mx;
	BYTE	*map;
} SixNode;

static FILE *out_fp = NULL;

static SixNode *node_top = NULL;
static SixNode *node_free = NULL;

static int save_pix = 0;
static int save_count = 0;

static int palet_max = 0;
static int palet_act = (-1);
static int palet_count[PALMAX];
static int palet_idx[PALMAX];
static int color_space = 2;

static int palet_hash = HASHMAX;
static int palet_mids = PALMAX / HASHMAX / 2;
static PalNode *palet_top[HASHMAX];
static PalNode palet_tab[PALMAX];

static int map_width = 0;
static int map_height = 0;
static BYTE *map_buf = NULL;

extern int maxPalet;
extern int maxValue[4];
extern int optTrue;
extern int optFill;
extern int resWidth;
extern int resHeight;
extern int optIndex;
extern int optNoise;
extern int optTransCol;
extern int optDrcs;
extern char optDscs[4];
extern int optDcss;
extern int optDcn;
extern int optFontW;
extern int optFontH;

/*********************************************************/

char *SixelStr(int val)
{
    static char buf[256];
    static char *p = buf + 256;

    if ( p < (buf + 16) )
	p = buf + 256;

    *(--p) = '\0';
    while ( val > 0 ) {
	*(--p) = '?' + (val & 63);
	val >>= 6;
    }
    return p;
}

/*********************************************************/

static void PutUtf8(DCHAR ch)
{
    int i;
    int n = 0;
    DCHAR c = ch;
    int tmp[8];

    tmp[n++] = 0x80 | (c & 0x3F);
    c >>= 6;

    if ( c <= 0x1F )
	tmp[n++] = 0xC0 | c;
    else {
	tmp[n++] = 0x80 | (c & 0x3F);
	c >>= 6;

	if ( c <= 0x0F )
	    tmp[n++] = 0xE0 | c;
	else {
	    tmp[n++] = 0x80 | (c & 0x3F);
	    c >>= 6;

	    if ( c <= 0x07 )
		tmp[n++] = 0xF0 | c;
	    else {
		tmp[n++] = 0x80 | (c & 0x3F);
		c >>= 6;

		if ( c < 0x03 )
		    tmp[n++] = 0xF8 | c;
		else {
		    tmp[n++] = 0x80 | (c & 0x3F);
		    c >>= 6;

		    tmp[n++] = 0xFC | c;
		}
	    }
	}
    }

    while ( n > 0 )
	fputc(tmp[--n], out_fp);
}

static void PutDrcs(int width, int height)
{
    int x, y;
    int S = 0, I = 0, X = 0, F = 0, C = 0;
    char *p = optDscs;
    DCHAR uc = 0;

    while ( *p != '\0' ) {
	if ( *p >= 0x20 && *p <= 0x2F ) {
	    I = X;
	    X = *(p++);
	} else if ( *p >= 0x30 && *p <= 0x7E ) {
	    F = *(p++);
	    break;
	}
    }
    C = optDcn + 0x20;
    S = optDcss == 0 ? 0x7E : 0x7F;

    switch(optDrcs % 10) {
    case 0:
	fprintf(out_fp, "\0337\033%c%s", (optDcss == 0 ? '(' : ','), optDscs);
	break;
    case 1:	// DRCSMMv2
	if ( I == 0 && X == 0x20 )
	    uc = 0x100000 + (F << 8) | (optDcss == 0 ? 0x00 : 0x80) | C;
	else
	    uc = 0x100000;
	break;
    case 2:	// DRCSMMv3
	if ( I == 0x20 && X >= 0x20 && X <= 0x2F && F >= 0x40 && F <= 0x7E && C >= 0x21 )
	    uc = 0x100000 + ((X - 0x20) * 63 + (F - 0x40)) * 94 + (C - 0x21);
	else
	    uc = 0x100000;
	break;
    default:
	return;
    }

    for ( y = 0 ; y < height ; y++ ) {
	for ( x = 0 ; x < width ; x++ ) {
	    switch(optDrcs % 10) {
	    case 0:
		if ( C > S ) {
		    if ( ++F < 0x30 )
			F = 0x30;
		    else if ( F > 0x7E ) {
			F = 0x30;

			if ( ++X < 0x20 )
			    X = 0x20;
			else if ( X > 0x2F ) {
			    X = 0x20;

			    if ( ++I < 0x20 )
				I = 0x20;
			    else if ( I > 0x2F )
				I = 0x20;
			}
		    }
		    fprintf(out_fp, "\033%c", (optDcss == 0 ? '(' : ','));
		    if ( I > 0 )
			fputc(I, out_fp);
		    if ( X > 0 )
			fputc(X, out_fp);
		    if ( F > 0 )
			fputc(F, out_fp);
		    C = (optDcss == 0 ? 0x21 : 0x20);
		}
		fputc(C++, out_fp);
		break;
	    case 1:
	    	PutUtf8(uc);
		F = (uc >> 8) & 0xFF;
		S = uc & 0x80;
		C = uc & 0x7F;
		if ( ++C > 0x7F ) {
		    C = 0x20;
		    if ( ++F > 0x7E ) {
			F = 0x30;
			S ^= 0x80;
		    }
		}
	    	uc = 0x100000 + (F << 8) | S | C;
		break;
	    case 2:
		PutUtf8(uc++);
		break;
	    }
	}
	fputc('\n', out_fp);
    }

    if ( (optDrcs % 10) == 0 )
	fprintf(out_fp, "\033(B\0338\033[%dB", height);
}

/*********************************************************/

static void PutData(int ch)
{
    fputc(ch, out_fp);
}
static void PutStr(char *str)
{
    fputs(str, out_fp);
}
static void PutFmt(char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   vfprintf(out_fp, fmt, ap);
   va_end(ap);
}

static void PutFlash()
{
    int n;

#ifdef	USE_VT240	// VT240 Max 255 ?
    while ( save_count > 255 ) {
	PutFmt("!%d%c", 255, save_pix);
	save_count -= 255;
    }
#endif

    if ( save_count > 3 ) {
	// DECGRI Graphics Repeat Introducer		! Pn Ch

	PutFmt("!%d%c", save_count, save_pix);

    } else {
	for ( n = 0 ; n < save_count ; n++ )
	    PutData(save_pix);
    }

    save_pix = 0;
    save_count = 0;
}
static void PutPixel(int pix)
{
    if ( pix < 0 || pix > 63 )
	pix = 0;

    pix += '?';

    if ( pix == save_pix ) {
	save_count++;
    } else {
	PutFlash();
	save_pix = pix;
	save_count = 1;
    }
}
static void PutPalet(int idx)
{
    // DECGCI Graphics Color Introducer			# Pc ; Pu; Px; Py; Pz

    if ( (palet_tab[idx].init & 005) == 4 ) {
	PutFmt("#%d;%d;%d;%d;%d", palet_tab[idx].idx, color_space,
		palet_tab[idx].rgba[1],
		palet_tab[idx].rgba[2],
		palet_tab[idx].rgba[3]);

	if ( palet_tab[idx].rgba[0] != 0 )
	    PutFmt(";%d", maxValue[0] - palet_tab[idx].rgba[0]);

	palet_tab[idx].init |= 001;

#ifdef	USE_VT240	// VT240 Update Palette Only ?
	PutFmt("#%d", palet_tab[idx].idx);
#endif
    } else if ( palet_act != idx )
	PutFmt("#%d", palet_tab[idx].idx);

    palet_act = idx;
}
static void PutCr()
{
    // DECGCR Graphics Carriage Return

    PutStr("$");
    // x = 0;
}
static void PutLf()
{
    // DECGNL Graphics Next Line

    PutStr("-\n");
    // x = 0;
    // y += 6;
}

/*********************************************************/

static void NodeFree()
{
    SixNode *np;

    while ( (np = node_free) != NULL ) {
	node_free = np->next;
	free(np);
    }
}
static void NodeDel(SixNode *np)
{
    SixNode *tp;

    if ( (tp = node_top) == np )
	node_top = np->next;

    else {
	while ( tp->next != NULL ) {
	    if ( tp->next == np ) {
		tp->next = np->next;
		break;
	    }
	    tp = tp->next;
	}
    }

    np->next = node_free;
    node_free = np;
}
static void NodeAdd(int pal, int sx, int mx, BYTE *map, int cmp)
{
    SixNode *np, *tp, top;

    if ( (np = node_free) != NULL )
	node_free = np->next;
    else if ( (np = (SixNode *)malloc(sizeof(SixNode))) == NULL )
	return;

    np->pal = pal;
    np->sx = sx;
    np->mx = mx;
    np->map = map;

    top.next = node_top;
    tp = &top;

    while ( tp->next != NULL ) {
	if ( cmp && np->pal != tp->next->pal )
	    break;
	if ( np->sx < tp->next->sx )
	    break;
	else if ( np->sx == tp->next->sx && np->mx > tp->next->mx )
	    break;
	tp = tp->next;
    }

    np->next = tp->next;
    tp->next = np;
    node_top = top.next;
}
static int NodeLine(int pal, BYTE *map, int cmp)
{
    int sx, mx, n;
    int count = 0;

    for ( sx = 0 ; sx < map_width ; sx++ ) {
	if ( map[sx] == 0 )
	    continue;

	for ( mx = sx + 1 ; mx < map_width ; mx++ ) {
	    if ( map[mx] != 0 )
		continue;

	    for ( n = 1 ; (mx + n) < map_width ; n++ ) {
		if ( map[mx + n] != 0 )
		    break;
	    }

	    if ( n >= 10 || (mx + n) >= map_width )
		break;
	    mx = mx + n - 1;
	}

	NodeAdd(pal, sx, mx, map, cmp);
	sx = mx - 1;
	count++;
    }
    return count;
}

/*********************************************************/

static int NodePut(gdImagePtr im, int x, SixNode *np)
{
    PutPalet(np->pal);
	
    for ( ; x < np->sx ; x++ )
	PutPixel(0);

    for ( ; x < np->mx ; x++ )
	PutPixel(np->map[x]);

    PutFlash();

    return x;
}
static void NodeFlush(gdImagePtr im, int optFill)
{
    int n, x, idx;
    SixNode *np, *next;
    BYTE *src, *dis;

    for ( n = 0 ; n < palet_max ; n++ )
	NodeLine(n, map_buf + map_width * n, 0);

    if ( optFill ) {
    	memset(palet_count, 0, sizeof(palet_count));

	for ( np = node_top ; np != NULL ; np = np->next ) {
	    for ( x = np->sx + 1 ; x < np->mx ; x++ ) {
		if ( np->map[x - 1] != np->map[x] )
		    palet_count[np->pal]++;
	    }
	}

	for ( idx = 0, n = 1 ; n < palet_max ; n++ ) {
	    if ( palet_count[idx] < palet_count[n] )
		idx = n;
	}

	dis = map_buf + map_width * idx;

	for ( np = node_top ; np != NULL ; np = next ) {
	    next = np->next;
	    if ( np->pal == idx )
	    	NodeDel(np);
	}

	for ( n = 0 ; n < palet_max ; n++ ) {
	    if ( n == idx )
		continue;
    	    src = map_buf + map_width * n;
	    for ( x = 0 ; x < map_width ; x++ )
		dis[x] |= src[x];
	}

	NodeLine(idx, dis, 1);
    }

    for ( x = 0 ; node_top != NULL ; ) {
	if ( x > node_top->sx ) {
	    PutCr();
	    x = 0;
	}

	x = NodePut(im, x, node_top);
	NodeDel(node_top);

	for ( np = node_top ; np != NULL ; np = next ) {
	    next = np->next;
	    if ( np->sx < x )
		continue;
	    x = NodePut(im, x, np);
	    NodeDel(np);
	}
    }

    for ( n = 0 ; n < palet_max ; n++ )
	palet_tab[n].init &= ~002;

    memset(map_buf, 0, palet_max * map_width);
}

/*********************************************************/

static void PalRGB(int rgb, PalNode *pPal)
{
    pPal->rgba[0] = PALVAL(gdTrueColorGetAlpha(rgb), maxValue[0], gdAlphaMax);
    pPal->rgba[1] = PALVAL(gdTrueColorGetRed  (rgb), maxValue[3], gdRedMax  );
    pPal->rgba[2] = PALVAL(gdTrueColorGetGreen(rgb), maxValue[2], gdGreenMax);
    pPal->rgba[3] = PALVAL(gdTrueColorGetBlue (rgb), maxValue[1], gdBlueMax );
}
static void PalInit(gdImagePtr im, int max, int back)
{
    int n, i, hs = 0;
    int rgb, idx = 0;
    PalNode *tp, tmp;

    for ( n = 0 ; n < HASHMAX ; n++ )
	palet_top[n] = NULL;

    if ( max > PALMAX )
	max = PALMAX;

    for ( palet_hash = HASHMAX ; palet_hash > 1 && (max / palet_hash) < 32 ; )
	palet_hash /= 2;

    palet_mids = max / palet_hash / 2;

    if ( gdImageTrueColor(im) ) {
        for ( n = 0 ; n < max ; n++ ) {
	    palet_tab[n].idx = n;
	    palet_tab[n].init = 0;
	    memset(palet_tab[n].rgba,  0xFF, sizeof(WORD) * 4);

	    palet_tab[n].next = palet_top[hs];
	    palet_top[hs] = &(palet_tab[n]);

	    if ( ++hs >= palet_hash )
	       hs = 0;

	    palet_idx[n] = n;
        }

    } else {
        for ( n = 0 ; n < max ; n++ ) {
	    rgb = (gdImageAlpha(im, n) << 24) |
	          (gdImageRed  (im, n) << 16) |
	          (gdImageGreen(im, n) <<  8) |
	           gdImageBlue (im, n);

    	    if ( n == back || gdTrueColorGetAlpha(rgb) == gdAlphaTransparent ) {
		palet_idx[n] = (-1);
	        continue;
	    }
	    if ( optTransCol != (-1) && (rgb & 0xFFFFFF) == optTransCol ) {
		palet_idx[n] = (-1);
	        continue;
	    }

	    palet_tab[idx].idx = idx;
	    palet_tab[idx].init = 006;
	    PalRGB(rgb, &(palet_tab[idx]));

    	    hs = 0;
    	    for ( i = 0 ; i < 4 ; i++ )
	        hs = hs * 31 + palet_tab[idx].rgba[i];
    	    hs &= (palet_hash - 1);

	    for ( tp = palet_top[hs] ; tp != NULL ; tp = tp->next ) {
		if ( memcmp(tp->rgba, palet_tab[idx].rgba, sizeof(WORD) * 4) == 0 )
		    break;
	    }

	    if ( tp == NULL ) {
	    	palet_idx[n] = idx;
    	    	palet_tab[idx].next = palet_top[hs];
    	    	palet_top[hs] = &(palet_tab[idx]);
	    	PutPalet(idx++);
	    } else
		palet_idx[n] = tp->idx;
	}

    	PutStr("\n");
	palet_act = (-1);
    }
}
static int PalAdd(gdImagePtr im, int x, int y)
{
    int n, hs, rgb, count;
    PalNode *bp, *tp, *mp, tmp;

    if ( !gdImageTrueColor(im) ) {
	if ( (n = gdImagePalettePixel(im, x, y)) >= 0 && n < palet_max )
	    return palet_idx[n];

	rgb = (gdImageAlpha(im, n) << 24) |
	      (gdImageRed  (im, n) << 16) |
	      (gdImageGreen(im, n) <<  8) |
	       gdImageBlue (im, n);

    } else
    	rgb = gdImageGetTrueColorPixel(im, x, y);

    if ( gdTrueColorGetAlpha(rgb) == gdAlphaTransparent )
	return (-1);
    if ( optTransCol != (-1) && (rgb & 0xFFFFFF) == optTransCol )
	return (-1);

    PalRGB(rgb, &tmp);

    hs = 0;
    for ( n = 0 ; n < 4 ; n++ )
	hs = hs * 31 + tmp.rgba[n];
    hs &= (palet_hash - 1);

    bp = &tmp;
    tp = bp->next = palet_top[hs];
    count = 0;

    for ( ; ; ) {
	if ( memcmp(tp->rgba, tmp.rgba, sizeof(WORD) * 4) == 0 )
	    goto ENDOF;

	if ( tp->next == NULL )
	    break;

	bp = tp;
	tp = tp->next;

	if ( ++count == palet_mids )
	    mp = bp;
    }

    if ( (tp->init & 002) != 0 ) {
	NodeFlush(im, 0);
	PutCr();
    }

    memcpy(tp->rgba, tmp.rgba, sizeof(WORD) * 4);
    tp->init = 006;
    bp->next = tp->next;

    tp->next = mp->next;
    mp->next = tp;
    return tp->idx;

ENDOF:
    tp->init |= 002;
    bp->next = tp->next;
    tp->next = tmp.next;
    palet_top[hs] = tp;
    return tp->idx;
}

/*********************************************************/

void gdImageSixel(gdImagePtr im, FILE *out)
{
    int n, i, x, y;
    int idx;
    int back = (-1);

    out_fp = out;

    map_width  = gdImageSX(im);
    map_height = gdImageSY(im);

    if ( maxPalet <= 0 )
	maxPalet = gdMaxColors;
    else if ( maxPalet > PALMAX )
	maxPalet = PALMAX;

    if ( optTrue ) {
        if ( !gdImageTrueColor(im) )
	    gdImagePaletteToTrueColor(im);

    	palet_max = maxPalet; 
        back = (-1);

    } else {
	if ( maxPalet > gdMaxColors )
	    maxPalet = gdMaxColors;

    	if ( !gdImageTrueColor(im) && gdImageColorsTotal(im) > maxPalet )
	    gdImagePaletteToTrueColor(im);

        if ( gdImageTrueColor(im) ) {
	    // poor ... but fast
	    //gdImageTrueColorToPaletteSetMethod(im, GD_QUANT_JQUANT, 0);
	    // debug version ?
	    //gdImageTrueColorToPaletteSetMethod(im, GD_QUANT_NEUQUANT, 9);

	    // used libimagequant/pngquant2 best !!
	    //gdImageTrueColorToPaletteSetMethod(im, GD_QUANT_LIQ, 0);

	    gdImageTrueColorToPalette(im, 1, maxPalet);
        }

    	palet_max = gdImageColorsTotal(im);
    	back = gdImageGetTransparent(im);
    }

    if ( (map_buf = (BYTE *)malloc(palet_max * map_width)) == NULL )
	return;

    palet_act = (-1);
    memset(map_buf, 0, palet_max * map_width);

    if ( optDrcs >= 0 && (optDrcs / 10) == 2 )
	goto SKIPDEFS;

    switch(optDrcs % 10) {
    case 0:
	PutFmt("\033[?8800l\033P0;%d;0;%d;0;3;%d;%d{%s", 
		optDcn, optFontW, optFontH, optDcss, optDscs);
	break;
    case 1:
	PutFmt("\033[?8800h\033[?8801l\033P0;%d;0;%d;0;3;%d;%d{%s", 
		optDcn, optFontW, optFontH, optDcss, optDscs);
	break;
    case 2:
	PutFmt("\033[?8800;8801h\033P0;%d;0;%d;0;3;%d;%d{%s", 
		optDcn, optFontW, optFontH, optDcss, optDscs);
	break;
    default:
        PutStr("\033P;;;");
        if ( optIndex >= 0 )
	    PutFmt("%d", optIndex);
        if ( optNoise > 0 )
	    PutFmt(";%d", optNoise);
        PutStr("q");
	break;
    }

    PutFmt("\"1;1;%d;%d\n", map_width, map_height);

    if ( maxValue[0] != 100 || maxValue[1] != 100 || 
	 maxValue[2] != 100 || maxValue[3] != 100 ) {
	color_space = 3;
    	PutFmt("*%d;%d;%d;%d;%d", color_space, 
		maxValue[3], maxValue[2], maxValue[1], maxValue[0]);
    }

    PalInit(im, palet_max, back);

    memset(palet_count, 0, sizeof(palet_count));

    for ( y = 0 ; y < map_height ; y += 6 ) {
	for ( x = 0 ; x < map_width ; x++ ) {
	    for ( i = 0 ; i < 6 && (y + i) < map_height ; i++ ) {
		idx = PalAdd(im, x, y + i);
		if ( idx >= 0 && idx < palet_max && idx != back )
		    map_buf[idx * map_width + x] |= (1 << i);
	    }
	}
	NodeFlush(im, optTrue ? 0 : optFill);
	PutLf();
    }

    PutStr("\033\\");

SKIPDEFS:

    if ( optDrcs >= 0 && (optDrcs / 10) != 1 )
    	PutDrcs((map_width + optFontW - 1) / optFontW, (map_height + optFontH - 1) / optFontH);

    NodeFree();
    free(map_buf);
}
