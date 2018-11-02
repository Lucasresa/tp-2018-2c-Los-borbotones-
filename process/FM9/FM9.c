#include "FM9.h"

int main(){
	log_FM9 = log_create("FM9.log","FM9",true,LOG_LEVEL_INFO);

	file_FM9=config_create("CONFIG_FM9.cfg");

	config_FM9.puerto_fm9=config_get_int_value(file_FM9,"PUERTO");
	config_FM9.modo=detectarModo(config_get_string_value(file_FM9,"MODO"));
	config_FM9.tamanio=config_get_int_value(file_FM9,"TAMANIO");
	config_FM9.max_linea=config_get_int_value(file_FM9,"MAX_LINEA");
	config_FM9.tam_pagina=config_get_int_value(file_FM9,"TAM_PAGINA");

	config_destroy(file_FM9);

	// Inicializo memoria.
	void *base_memoria;
	base_memoria = malloc(config_FM9.tamanio);

	// Creo estructuras de segmentación
	t_list *lista_tablas_segmentos;
	lista_tablas_segmentos = list_create();

	// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
	// %%%% EJEMPLO MANIPULANDO LAS TABLAS DE SEGMENTACION %%%%
	// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

	// Creo ejemplo de tabla de segmentación
	list_add(lista_tablas_segmentos, list_create());

	// Agrego una entrada a la tabla de segmentos
	t_list *tabla_segmentos = list_get(lista_tablas_segmentos, 0);
	list_add(tabla_segmentos, crear_fila_tabla_seg(5,1,1));

	// Obtengo el id de la fila que acabo de agregar
	t_list *tabla_segmentos_copia = list_get(lista_tablas_segmentos, 0);
	fila_tabla_seg *fila_tabla_seg;
	fila_tabla_seg = list_get(tabla_segmentos_copia, 0);
	// printf("%i",fila_tabla_seg->id_segmento);

	// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
	// %%%%%%%%%%%%%%%%%%% FIN EJEMPLO %%%%%%%%%%%%%%%%%%%%%%%%
	// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%



	int listener_socket;
	crearSocket(&listener_socket);

	setearParaEscuchar(&listener_socket, config_FM9.puerto_fm9);

	log_info(log_FM9, "Escuchando conexiones...");

    pthread_t thread_id;

    pthread_create(&thread_id, NULL, consolaThread, NULL);

	fd_set fd_set;
	FD_ZERO(&fd_set);
	FD_SET(listener_socket, &fd_set);
	while(true) {
		escuchar(listener_socket, &fd_set, &funcionHandshake, NULL, &funcionRecibirPeticion, NULL );
		// Probablemente conviene sacar el sleep()
		sleep(1);
	}
	free(base_memoria);
	list_destroy(lista_tablas_segmentos);

}

struct fila_tabla_seg *crear_fila_tabla_seg(int id_segmento, int limite_segmento, int base_segmento) {
	   struct fila_tabla_seg *p;
	   p = (struct fila_tabla_seg *) malloc(sizeof(struct fila_tabla_seg));
	   p->id_segmento = id_segmento;
	   p->limite_segmento = limite_segmento;
	   p->base_segmento = base_segmento;
	   return p;
}

void *consolaThread(void *vargp)
{
	while(true) {
		char *line = readline("$ ");

		if (*line==NULL) continue;

		char *command = strtok(line, " ");

		if (strcmp(command, "dump")==0) {
			char *value = strtok(0, " ");
			if (value==NULL) {
				puts("Please insert an id to dump");
				continue;
			}
			printf("Structures of process with id: %s \n", value);

		} else {
			puts("Command not recognized.");
		}
	}
}

t_modo detectarModo(char* modo){

	t_modo modo_enum;

	if(string_equals_ignore_case(modo,"TPI")){
		modo_enum=TPI;
	}else if(string_equals_ignore_case(modo,"SPA")){
		modo_enum=SPA;
	}else {
		modo_enum=SEG;
	}
	return modo_enum;
}

void funcionHandshake(int socket, void* argumentos) {
	log_info(log_FM9, "Conexión establecida");
	return;
}

void funcionRecibirPeticion(int socket, void* argumentos) {
	// Acá estaría la lógica para manejar un pedido.
	return;
}
