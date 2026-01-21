CC = gcc
CFLAGS = -Wall -O2
LDFLAGS = -libverbs

all: server client

server: server.c rdma_common.h
	$(CC) $(CFLAGS) -o $@ server.c $(LDFLAGS)

client: client.c rdma_common.h
	$(CC) $(CFLAGS) -o $@ client.c $(LDFLAGS)

clean:
	rm -f server client
