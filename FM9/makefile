CC=gcc

CFLAGS=src/FM9.c ../biblioteca/bibliotecaDeSockets.c src/segmentacion.c src/seg_paginada.c

install: $(CFLAGS)
	$(CC) $(CFLAGS) -o FM9 -lcommons -lpthread -lreadline -lm

.PHONY: clean

clean:
	-rm FM9 $(objects)