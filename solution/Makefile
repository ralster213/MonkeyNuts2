BINS = wfs mkfs
BADSTUFF = disk1 disk2 mnt/
CC = gcc
CFLAGS = -Wall -Werror -pedantic -std=gnu18 -g
FUSE_CFLAGS = `pkg-config fuse --cflags --libs`


.PHONY: all
all: $(BINS)

wfs:
	$(CC) $(CFLAGS) wfs.c $(FUSE_CFLAGS) -o wfs
mkfs:
	$(CC) $(CFLAGS) -o mkfs mkfs.c

.PHONY: clean
clean:
	rm -rf $(BINS)
	rm -rf $(BADSTUFF)

fuse0:
	mkdir mnt
	./mkfs -r 0 -d disk1 -d disk2 -i 32 -b 200
	./wfs disk1 disk2 -f -s mnt

fuse1:
	mkdir mnt
	./mkfs -r 1 -d disk1 -d disk2 -i 32 -b 200
	./wfs disk1 disk2 -f -s mnt