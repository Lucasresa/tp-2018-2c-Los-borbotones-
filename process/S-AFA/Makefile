CC=gcc

CFLAGS=S-AFA.c load_config.c

CAUX=Parser/parser.c gestor_GDT/gestor_GDT.c ../../utils/bibliotecaDeSockets.c 

safa: $(CFLAGS) $(CAUX)
		$(CC) $(CFLAGS) $(CAUX) -o safa -lcommons -lpthread -lreadline

