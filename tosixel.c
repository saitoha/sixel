#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <gd.h>

#define PALMAX		1024
#define HASHMAX		8

#define	PALVAL(n,a,m)	(((n) * (a) + ((m) / 2)) / (m))

#if USE_TRUECOLOR
#define	RGBMASK		0xFFFFFFFF
#else
#define	RGBMASK		0xFFFEFEFE
#endif

typedef unsigned char BYTE;

typedef struct _PalNode {
	struct _PalNode *next;
	int	idx;
	int	rgb;
	int	init;
} PalNode;

typedef struct _SixNode {
	struct _SixNode *next;
	int	pal;
	int	sx;
	int	mx;
	BYTE *map;
} SixNode;

static FILE *out_fp = NULL;

static SixNode *node_top = NULL;
static SixNode *node_free = NULL;

static int save_pix = 0;
static int save_count = 0;
static int act_palet = (-1);

static PalNode	*palet_top[HASHMAX];
static PalNode	palet_tab[PALMAX];

static int	palet_hash = HASHMAX;
static int	palet_max = PALMAX;
static int	map_width = 0;
static int	map_height = 0;
static BYTE	*bitmap = NULL;

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

static void PutFlush()
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
	PutFlush();
	save_pix = pix;
	save_count = 1;
    }
}
static void PutPalet(gdImagePtr im, int idx)
{
    // DECGCI Graphics Color Introducer			# Pc ; Pu; Px; Py; Pz

    if ( (palet_tab[idx].init & 001) == 0 ) {
#if USE_TRUECOLOR
    	PutFmt("#%d;3;%d;%d;%d", idx,
		gdTrueColorGetRed  (palet_tab[idx].rgb), 
		gdTrueColorGetGreen(palet_tab[idx].rgb), 
		gdTrueColorGetBlue (palet_tab[idx].rgb));
#else
    	PutFmt("#%d;2;%d;%d;%d", idx,
		PALVAL(gdTrueColorGetRed  (palet_tab[idx].rgb), 100, gdRedMax  ), 
		PALVAL(gdTrueColorGetGreen(palet_tab[idx].rgb), 100, gdGreenMax  ), 
		PALVAL(gdTrueColorGetBlue (palet_tab[idx].rgb), 100, gdBlueMax  ));
#endif
    	palet_tab[idx].init |= 1;

    } else if ( act_palet != idx )
    	PutFmt("#%d", idx);

    act_palet = idx;
}
static void PutCr()
{
    // DECGCR Graphics Carriage Return

    PutStr("$\n");
    // x = 0;
}
static void PutLf()
{
    // DECGNL Graphics Next Line

    PutStr("-\n");
    // x = 0;
    // y += 6;
}

/****************************************************************/

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
static void NodeAdd(int pal, int sx, int mx, BYTE *map)
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
static void NodeLine(int pal, int width, BYTE *map)
{
    int sx, mx, n;

    for ( sx = 0 ; sx < width ; sx++ ) {
	if ( map[sx] == 0 )
	    continue;

	for ( mx = sx + 1 ; mx < width ; mx++ ) {
	    if ( map[mx] != 0 )
		continue;

	    for ( n = 1 ; (mx + n) < width ; n++ ) {
		if ( map[mx + n] != 0 )
		    break;
	    }

	    if ( n >= 10 || (mx + n) >= width )
		break;
	    mx = mx + n - 1;
	}

	NodeAdd(pal, sx, mx, map);
	sx = mx - 1;
    }
}
static int PutNode(gdImagePtr im, int x, SixNode *np)
{
    PutPalet(im, np->pal);
	
    for ( ; x < np->sx ; x++ )
	PutPixel(0);

    for ( ; x < np->mx ; x++ )
	PutPixel(np->map[x]);

    PutFlush();

    return x;
}
static void NodeFlush(gdImagePtr im)
{
    int x;
    SixNode *np;

    for ( x = 0 ; x < palet_max ; x++ ) {
	NodeLine(x, map_width, bitmap + x * map_width);
	palet_tab[x].init &= 001;
    }

    for ( x = 0 ; (np = node_top) != NULL ; ) {
	if ( x > np->sx ) {
	    PutCr();
	    x = 0;
	}

	x = PutNode(im, x, np);
	NodeDel(np);
	np = node_top;

	while ( np != NULL ) {
	    if ( np->sx < x ) {
		np = np->next;
		continue;
	    }

	    x = PutNode(im, x, np);
	    NodeDel(np);
	    np = node_top;
	}
    }
    
    memset(bitmap, 0, map_width * palet_max);
}

/****************************************************************/

static void PalInit(int max)
{
    int n, hs = 0;

    for ( n = 0 ; n < HASHMAX ; n++ )
    	palet_top[n] = NULL;

    if ( max > PALMAX )
	max = PALMAX;

    for ( palet_hash = HASHMAX ; palet_hash > 0 && (max / palet_hash) < 16 ; )
	palet_hash /= 2;

    for ( n = max - 1 ; n >= 0 ; n-- ) {
    	palet_tab[n].idx = n;
        palet_tab[n].rgb = 0xFF000000;
        palet_tab[n].init = 0;

    	palet_tab[n].next = palet_top[hs];
    	palet_top[hs] = &(palet_tab[n]);

	if ( ++hs >= palet_hash )
	    hs = 0;
    }
}

static int PalAdd(gdImagePtr im, int x, int y)
{
    int hs, rgb;
    PalNode *bp, *tp, tmp;

    rgb = gdImageGetTrueColorPixel(im, x, y) & RGBMASK;
    hs = (rgb * 31) & (palet_hash - 1);

    bp = &tmp;
    tp = bp->next = palet_top[hs];

    for ( ; ; ) {
	if ( tp->rgb == rgb )
	    goto ENDOF;

	if ( tp->next == NULL )
	    break;

	bp = tp;
	tp = tp->next;
    }

    if ( (tp->init & 002) != 0 ) {
	NodeFlush(im);
    	PutCr();
    }

    tp->rgb = rgb;
    tp->init = 0;

ENDOF:
    tp->init |= 002;
    bp->next = tp->next;
    tp->next = tmp.next;
    palet_top[hs] = tp;
    return tp->idx;
}

/****************************************************************/

void gdImageSixel(gdImagePtr im, FILE *out, int maxPalet, int optPalet)
{
    int x, y, i;
    int idx;
    SixNode *np;

    out_fp = out;

    map_width  = gdImageSX(im);
    map_height = gdImageSY(im);

    if ( (palet_max = maxPalet) > PALMAX || palet_max <= 0 )
	maxPalet = PALMAX;

    if ( !gdImageTrueColor(im) )
	gdImagePaletteToTrueColor(im);

    PalInit(palet_max);

    if ( (bitmap = (BYTE *)malloc(map_width * palet_max)) == NULL )
	return;

    memset(bitmap, 0, map_width * palet_max);

    PutFmt("\033Pq\"1;1;%d;%d\n", map_width, map_height);

    for ( y = 0 ; y < map_height ; y += 6 ) {
	for ( x = 0 ; x < map_width ; x++ ) {
	    for ( i = 0 ; i < 6 && (y + i) < map_height ; i++ ) {
	    	idx = PalAdd(im, x, y + i);
		if ( gdTrueColorGetAlpha(palet_tab[idx].rgb) == gdAlphaOpaque )
	    	    bitmap[idx * map_width + x] |= (1 << i);
	    }
	}

	NodeFlush(im);
	PutLf();
    }

    PutStr("\033\\");

    NodeFree();
    free(bitmap);
}
