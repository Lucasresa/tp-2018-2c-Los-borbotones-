CC=gcc

CFLAGS=src/FM9.c ../biblioteca/bibliotecaDeSockets.c src/segmentacion.c

install: $(CFLAGS)
	$(CC) $(CFLAGS) -o FM9 -lcommons -lpthread -lreadline

.PHONY: clean

clean:
	-rm FM9 $(objects)