CC = cc
CFLAGS = -O3 -g -I/usr/local/include
LFLAGS = -g -L/usr/local/lib -lgd

# vt240 sixel compatible ?
#CFLAGS+= -DUSE_VT240

OBJS = main.o tosixel.o fromsixel.o frompnm.o
PROG = sixel

$(PROG)	:	$(OBJS)
	$(CC) $(OBJS) $(LFLAGS) -o $(PROG)

.c.o	:
	$(CC) $(CFLAGS) -c $<
