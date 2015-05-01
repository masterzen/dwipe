GCC?=gcc

all: *.c
	#$(CC) -Os -Wall -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE *.c libncurses.a -o dwipe
	$(GCC) -Os -Wall -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE *.c -lncurses -o dwipe
	#$(STRIP) dwipe

clean:
	rm -f a.out dwipe
	rm -f *.o

# eof
