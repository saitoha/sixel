CC = cc
CFLAGS = -O3 -g -I/usr/local/include
LFLAGS = -L/usr/local/lib -lgd

# vt240 sixel compatible ?
#CFLAGS+= -DUSE_VT240

# xterm sixel compatible ?
#CFLAGS+= -DUSE_INITPAL

# sixel true color extend
#CFLAGS+= -DUSE_TRUECOLOR

OBJS = main.o tosixel.o fromsixel.o frompnm.o
PROG = sixel

$(PROG)	:	$(OBJS)
	$(CC) $(OBJS) $(LFLAGS) -o $(PROG)

.c.o	:
	$(CC) $(CFLAGS) -c $<
