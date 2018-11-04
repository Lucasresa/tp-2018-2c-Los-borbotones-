CC=gcc

CFLAGS=S-AFA.c load_config.c

CAUX=gestor_GDT/gestor_GDT.c ../../utils/bibliotecaDeSockets.c 

safa: $(CFLAGS) $(CAUX)
		$(CC) $(CFLAGS) $(CAUX) -o safa -lcommons -lpthread -lreadline #Compilo el proceso S-AFA

.PHONY : clean

clean:
	-rm safa $(objects)