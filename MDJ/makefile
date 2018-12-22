CC=gcc

CFLAGS=src/MDJ.c ../biblioteca/bibliotecaDeSockets.c

install: $(CFLAGS)
	$(CC) $(CFLAGS) -o MDJ -lcrypto -lcommons -lpthread -lreadline #Compilo el proceso MDJ

.PHONY: clean
clean:
	-rm MDJ $(objects)
