

LIBDIR= -L../libdwarf
LIBS= -ldwarf -lelf
# FreeBSD gcc has -g -gdwarf-2 not -gdwarf-4
CFLAGS= -g -Wall -I../libdwarf

all: simplereader frame1

simplereader: simplereader.c
	$(CC) $(CFLAGS) simplereader.c -o simplereader $(LIBDIR) $(LIBS)
frame1: frame1.c
	$(CC) $(CFLAGS) frame1.c -o frame1 $(LIBDIR) $(LIBS)

clean:
	rm -f simplereader.o
	rm -f simplereader
	rm -f frame1
	rm -f frame1.o
