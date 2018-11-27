CC=gcc

CFLAGS=src/CPU.c ../biblioteca/bibliotecaDeSockets.c

CAUX=src/Parser/parser.c 

install: $(CFLAGS) $(CAUX)
	$(CC) $(CFLAGS) $(CAUX) -o CPU -lcommons -lpthread #Compilo el proceso CPU

.PHONY: clean

clean:
	-rm CPU $(objects)