CC=gcc
CFLAGS=-Wall -std=c2x -g -fsanitize=address
LDFLAGS=-lm -lpthread
INCLUDE=-Iinclude

.PHONY: clean

pkgmain: src/pkgmain.c src/chk/pkgchk.c src/tree/merkletree.c src/crypt/sha256.c
	$(CC) $^ $(INCLUDE) $(CFLAGS) $(LDFLAGS) -o $@

btide: src/btide.c src/package.c src/config.c src/peer.c src/packet.c src/chk/pkgchk.c src/tree/merkletree.c src/crypt/sha256.c
	$(CC) $^ $(INCLUDE) $(CFLAGS) $(LDFLAGS) -o $@

p1tests:
	bash p1test.sh

p2tests:
	bash p2test.sh

clean:
	rm -f *.o pkgmain btide
