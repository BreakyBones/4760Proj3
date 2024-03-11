CC	= gcc -g3
CFLAGS  = -g3

all: oss worker

oss: oss.c
	$(CC) $(CFLAGS) -o $@ $<

user: worker.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f oss worker
