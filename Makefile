GCC?=gcc
STRIP?=strip

all: *.c
	$(GCC) -Os -Wall -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE *.c -lncurses -o dwipe
	#$(STRIP) dwipe

clean:
	rm -f dwipe
