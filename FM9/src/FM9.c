#include "FM9.h"
#include <math.h>


int main(){
	log_FM9 = log_create("FM9.log","FM9",true,LOG_LEVEL_INFO);


    char *archivo;
	archivo="src/CONFIG_FM9.cfg";

	if(validarArchivoConfig(archivo) <0)
		return-1;

	file_FM9=config_create(archivo);

	config_FM9.puerto_fm9=config_get_int_value(file_FM9,"PUERTO");
	config_FM9.modo=detectarModo(config_get_string_value(file_FM9,"MODO"));
	config_FM9.tamanio=config_get_int_value(file_FM9,"TAMANIO");
	config_FM9.max_linea=config_get_int_value(file_FM9,"MAX_LINEA");
	config_FM9.tam_pagina=config_get_int_value(file_FM9,"TAM_PAGINA");

	config_destroy(file_FM9);

	// Memoria es un array de strings, con el tamaño de cada string igual a MAX_LINEA.
	memoria = iniciar_memoria();

	pthread_t thread_id;

	if (config_FM9.modo == SEG) {
		// Abro la consola
		pthread_create(&thread_id, NULL, consolaThreadSeg, NULL);
		// Creo estructuras de segmentación
		lista_tablas_segmentos = list_create();
		tabla_segmentos_pid = list_create();
		memoria_vacia_seg = list_create();
		list_add(memoria_vacia_seg, crear_fila_mem_vacia_seg(0, config_FM9.tamanio/config_FM9.max_linea));
	} else if (config_FM9.modo == TPI) {
		// Creo estructuras de paginación invertida
		crearTablaPagInv();
	}

	int listener_socket;
	crearSocket(&listener_socket);

	setearParaEscuchar(&listener_socket, config_FM9.puerto_fm9);

	log_info(log_FM9, "Escuchando conexiones...");

	FD_ZERO(&set_fd);
	FD_SET(listener_socket, &set_fd);
	while(true) {
		escuchar(listener_socket, &set_fd, &funcionHandshake, NULL, &recibirPeticion, NULL );
	}

	// Limpio memoria
	int cantidad_lineas = config_FM9.tamanio / config_FM9.max_linea;
	for (int n=0; n<cantidad_lineas; n++) {
		free(memoria[n]);
	}
	free(memoria);
	list_destroy(lista_tablas_segmentos);

}

char** iniciar_memoria() {
	char** memoria;
	int cantidad_lineas = config_FM9.tamanio / config_FM9.max_linea;

	memoria = malloc(cantidad_lineas * sizeof(memoria));

	for (int n=0; n<cantidad_lineas; n++) {
		memoria[n] = malloc(config_FM9.max_linea * sizeof(char));
	}
	return memoria;
}

struct fila_pag_invertida *crear_fila_tabla_pag_inv(int indice, int pid, int pagina,int flag) {
	   struct fila_pag_invertida *p;
	   p = (struct fila_pag_invertida *) malloc(sizeof(struct fila_pag_invertida));
	   p->indice = indice;
	   p->pid = pid;
	   p->pagina = pagina;
	   p->flag = flag;
	   return p;
}

void *consolaThread(void *vargp)
{
	while(true) {
		char *line = readline("$ ");

		if (*line==(int)NULL) continue;

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

void* funcionHandshake(int socket, void* argumentos) {
	printf("conexion establecida con socket %i\n", socket);
	log_info(log_FM9, "Conexión establecida");
	return 0;
}

void* recibirPeticion(int socket, void* argumentos) {

	if (config_FM9.modo == SEG) {
		recibirPeticionSeg(socket);
	} else if (config_FM9.modo == TPI) {
		// Llamar acá a la función que resuelve peticiones en modo TPI
	}
	return 0;
}

int recibirPeticionPagInv(int socket) {
	int length;
	int header;
	length = recv(socket,&header,sizeof(int),0);
	if ( length <= 0 ) {
		close(socket);
		FD_CLR(socket, &set_fd);
		log_error(log_FM9, "Se perdió la conexión con un socket.");
		perror("error");
		return 0;
	}

	switch(header){
	case 4:
	{
		return 0;
	}
	case INICIAR_MEMORIA_PID:
	{
		iniciar_scriptorio_memoria* datos_script = recibirYDeserializar(socket,header);
		int pid = datos_script->pid;
		int tamanio_script = datos_script->size_script;

		int cant_lineas_pag = config_FM9.tam_pagina/config_FM9.max_linea;

		int cantidad_frames = tamanio_script/cant_lineas_pag;

		int frames_redondeados = ceil(cantidad_frames);

		int i = 0;

		// TODO: Busco en memoria espacio libre

		for( i = 0; i < frames_redondeados; i = i + 1 ){
			fila_pag_invertida* unaFilaDisponible = encontrarFilaVacia();
			unaFilaDisponible->pid = pid;
		}

		free(datos_script);

		int success=MEMORIA_INICIALIZADA;

		serializarYEnviarEntero(socket,&success);
		return 0;
	}
	case PEDIR_LINEA:
	{
		direccion_logica* direccion;
		//TODO: Cambiar PEDIR_DATOS a PEDIR_LINEA en la biblioteca?
		//direccion = recibirYDeserializar(socket,header);
		return 0;
	}
	case ESCRIBIR_LINEA:
	{
		cargar_en_memoria* info_a_cargar;

		int pagina = 0;

		info_a_cargar = recibirYDeserializar(socket,header);

		//Cargo en memoria con mi pagina y offset traducido
		cargarEnMemoriaPagInv(info_a_cargar->pid, traducirPagina(pagina,info_a_cargar->offset), traducirOffset(info_a_cargar->offset), info_a_cargar->linea, 1);

		free(info_a_cargar);
		return 0;

	}

	case ENVIAR_ARCHIVO:
	{
		//char* linea_string;
		//linea_string = recibirYDeserializar(socket,header);
		//strcpy(memoria[memoria_counter],linea_string);
		//printf("String recibido y guardado en linea %i: %s\n", memoria_counter, memoria[memoria_counter]);
		//memoria_counter++;
		return 0;
	}
	}
}

char* leerMemoriaPagInv(int pid, int pagina, int offset) {


	int es_pid_pagina_buscados(fila_pag_invertida *p) {
		return (p->pid == pid)&&(p->pagina == pagina);
	}

	//Que elemento de la lista cumple con ese pid y esa pagina que se pasan por parametro?

	fila_pag_invertida *fila_tabla_pag_inv = list_find(lista_tabla_pag_inv, (void*) es_pid_pagina_buscados);

	int marcoEncontrado = fila_tabla_pag_inv->indice+offset;

	printf("Leo memoria en posicion %i: '%s'\n", marcoEncontrado+offset, memoria[marcoEncontrado+offset]);

	return memoria[marcoEncontrado+offset];


}

int cargarEnMemoriaPagInv(int pid, int pagina, int offset, char* linea,int flag) {

	//Buscamos
	int es_pid_pagina_buscados(fila_pag_invertida *p) {
		return (p->pid == pid)&&(p->pagina == pagina);
	}

	//Que elemento de la lista cumple con ese pid y esa pagina que se pasan por parametro?
	fila_pag_invertida *fila_tabla_pag_inv = list_find(lista_tabla_pag_inv, (void*) es_pid_pagina_buscados);

	memoria[fila_tabla_pag_inv->indice+offset] = linea;
	printf("Escribí en Posición %i: '%s'\n", fila_tabla_pag_inv->indice+offset, memoria[fila_tabla_pag_inv->indice+offset]);
	return 0;

}

//Si el offset es mayor al tamaño maximo de una pagina, sumo la cantidad de paginas desplazadas a mi pagina, si no, no hago nada
//EJEMPLO: Si mi offset es 30 y mi tamaño maximo de pagina es 15, voy a estar en la pagina 2, ya que 30/15 es 2 y 0+2 = 2

int traducirPagina(int pagina,int offset){
	if (offset > config_FM9.tam_pagina){
		//la division del offset con el tamaño de la pagina me dice que tantas paginas se desplazo de la pagina 0
		return pagina + (offset/config_FM9.tam_pagina);
	}
	else{
		return pagina;
	}
}

//Verifico como queda el offset después de desplazarme a una pagina (EL valor siempre va a dar menor a 15).
//EJEMPLO: Si mi offset es 18 y mi tamaño maximo de pagina es 15, me va a quedar que el offset es 3

int traducirOffset(int offset){
	if (offset > config_FM9.tam_pagina){
		return offset - config_FM9.tam_pagina * (offset/config_FM9.tam_pagina);
	}
	else{
		return offset;
	}
}

void crearTablaPagInv(){

	int tamanio_memoria = config_FM9.tamanio;
	int tamanio_pagina = config_FM9.tam_pagina;
	int i;

	int cantidad_marcos = tamanio_memoria/tamanio_pagina;

	for( i = 0; i < cantidad_marcos; i = i + 1 ){
		list_add(lista_tabla_pag_inv,crear_fila_tabla_pag_inv(i,0,0,0));
	}
}

fila_pag_invertida* encontrarFilaVacia() {

	//Creamos la condicion de que el flag esté seteado en 0 (sin usar)
	int estaVacio(fila_pag_invertida *p){
		return (p->flag == 0);
	}
	//Que fila está sin usar?
	return list_find(lista_tabla_pag_inv, (void*) estaVacio);

}


