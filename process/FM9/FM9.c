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

	// Memoria es un array de strings de tamaño=MAX_LINEA.
	memoria = iniciar_memoria();

	// Creo estructuras de segmentación
	lista_tablas_segmentos = list_create();
	tabla_segmentos_pid = list_create();

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
/*
    pthread_t thread_id;

    pthread_create(&thread_id, NULL, consolaThread, NULL);
*/
	FD_ZERO(&set_fd);
	FD_SET(listener_socket, &set_fd);
	while(true) {
		escuchar(listener_socket, &set_fd, &funcionHandshake, NULL, &funcionRecibirPeticion, NULL );
	}
	free(memoria);
	list_destroy(lista_tablas_segmentos);

}

char** iniciar_memoria() {
	char** memoria;
	int cantidad_lineas = config_FM9.tamanio / config_FM9.max_linea;

	memoria_counter = 0;

	memoria = malloc(cantidad_lineas * sizeof(memoria));

	for (int n=0; n<16; n++) {
		memoria[n] = malloc(config_FM9.max_linea * sizeof(char));
	}
	return memoria;
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
	printf("conexion establecida con socket %i\n", socket);
	log_info(log_FM9, "Conexión establecida");
	return;
}

void funcionRecibirPeticion(int socket, void* argumentos) {
	int length;
	int header;
	length = recv(socket,&header,sizeof(int),0);
	if ( length <= 0 ) {
		close(socket);
		FD_CLR(socket, &set_fd);
		log_error(log_FM9, "Se perdió la conexión con un socket.");
		perror("error");
		return;
	}

	switch(header){
	case 4:
	{
		return;
	}
	case INICIAR_MEMORIA_PID:
	{
		int pid = *recibirYDeserializarEntero(socket);
		int size_scriptorio = 0;

		// Creo una tabla de segmentos
		list_add(lista_tablas_segmentos, list_create());

		// Obtengo el id de la tabla
		int id_tabla_segmentos = list_size(lista_tablas_segmentos)-1;

		// Relaciono la tabla de segmentos con el PID
		fila_tabla_segmentos_pid* relacion_pid_tabla = malloc(sizeof(fila_tabla_segmentos_pid));
		relacion_pid_tabla->id_proceso=pid;
		relacion_pid_tabla->id_tabla_segmentos=id_tabla_segmentos;
		list_add(tabla_segmentos_pid,relacion_pid_tabla);

		// Obtengo la tabla de segmentos recién creada
		t_list *tabla_segmentos = list_get(lista_tablas_segmentos, id_tabla_segmentos);
		// Busco en memoria espacio libre
		// Creo un segmento en la tabla
		list_add(tabla_segmentos, crear_fila_tabla_seg(0,size_scriptorio,mem_libre_base));

		int success=MEMORIA_INICIALIZADA;
		serializarYEnviarEntero(socket,&success);
		return;
	}
	case INICIAR_SCRIPTORIO:
	{
		puts("Recibo una linea");
		cargar_en_memoria* info_a_cargar;
		info_a_cargar = recibirYDeserializar(socket,header);
		printf("Linea recibida!: %s\n\n", info_a_cargar->linea);
		free(info_a_cargar);
		return;
	}

	case ENVIAR_ARCHIVO:
	{
		char* linea_string;
		linea_string = recibirYDeserializar(socket,header);
		strcpy(memoria[memoria_counter],linea_string);
		printf("String recibido y guardado en linea %i: %s\n", memoria_counter, memoria[memoria_counter]);
		memoria_counter++;
		return;
	}
	}
}
