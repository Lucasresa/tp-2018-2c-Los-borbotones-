CC=gcc

CFLAGS=CPU.c ../../utils/bibliotecaDeSockets.c

CAUX=Parser/parser.c 

cpu: $(CFLAGS) $(CAUX)
	$(CC) $(CFLAGS) $(CAUX) -o CPU -lcommons -lpthread #Compilo el proceso CPU
