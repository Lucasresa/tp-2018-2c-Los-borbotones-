CC=gcc

CFLAGS=src/S-AFA.c src/load_config.c

CAUX=src/gestor_GDT/gestor_GDT.c ../biblioteca/bibliotecaDeSockets.c 

install: $(CFLAGS) $(CAUX)
		$(CC) $(CFLAGS) $(CAUX) -o SAFA -lcommons -lpthread -lreadline #Compilo el proceso S-AFA

.PHONY: clean

clean:
	-rm SAFA $(objects)