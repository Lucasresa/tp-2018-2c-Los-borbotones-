#include "FM9.h"

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

	if (config_FM9.modo == SEG) {
		// Creo estructuras de segmentación
		lista_tablas_segmentos = list_create();
		tabla_segmentos_pid = list_create();
	} else if (config_FM9.modo == TPI) {
		// Creo estructuras de paginación invertida
		crearTablaPagInv();
	}

	/*
	// Creo el thread para la consola
	pthread_t thread_id;
	pthread_create(&thread_id, NULL, consolaThread, NULL);
	*/

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

	mem_libre_base = 0;

	memoria = malloc(cantidad_lineas * sizeof(memoria));

	for (int n=0; n<cantidad_lineas; n++) {
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

int recibirPeticionSeg(int socket) {
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
		int size_scriptorio = datos_script->size_script;

		// Creo una tabla de segmentos
		list_add(lista_tablas_segmentos, list_create());

		// Obtengo el id de la tabla
		int id_tabla_segmentos = list_size(lista_tablas_segmentos)-1;
		// Relaciono la tabla de segmentos con el PID
		fila_tabla_segmentos_pid* relacion_pid_tabla = malloc(sizeof(fila_tabla_segmentos_pid));
		relacion_pid_tabla->id_proceso=datos_script->pid;
		relacion_pid_tabla->id_tabla_segmentos=id_tabla_segmentos;
		list_add(tabla_segmentos_pid,relacion_pid_tabla);

		// Obtengo la tabla de segmentos recién creada
		t_list *tabla_segmentos = list_get(lista_tablas_segmentos, id_tabla_segmentos);

		// TODO: Busco en memoria espacio libre

		// Creo un segmento en la tabla
		printf("Creo un segmento en posicion %i\n", mem_libre_base);
		list_add(tabla_segmentos, crear_fila_tabla_seg(0,size_scriptorio,mem_libre_base));
		mem_libre_base = mem_libre_base+size_scriptorio+1;

		free(datos_script);

		int success=MEMORIA_INICIALIZADA;
		serializarYEnviarEntero(socket,&success);
		return 0;
	}
	case PEDIR_LINEA:
	{
		direccion_logica* direccion;
		direccion = recibirYDeserializar(socket,header);
		char* linea = leerMemoriaSeg(direccion->pid, direccion->base, direccion->offset);
		serializarYEnviarString(socket,linea);
		return 0;
	}
	case ESCRIBIR_LINEA:
	{
		cargar_en_memoria* info_a_cargar;
		info_a_cargar = recibirYDeserializar(socket,header);
		if (cargarEnMemoriaSeg(info_a_cargar->pid, info_a_cargar->id_segmento, info_a_cargar->offset, info_a_cargar->linea)<0) {
			log_error(log_FM9, "Se intentó cargar una memoria fuera del limite del segmento.");
		}
		free(info_a_cargar);
		int success=LINEA_CARGADA;
		serializarYEnviarEntero(socket,&success);
		return 0;
	}
	case CERRAR_ARCHIVO:
	{
		direccion_logica* direccion;
		direccion = recibirYDeserializar(socket,header);
		int id_segmento = direccion->base;

		// Busco la tabla de segmentos del proceso:
		t_list* tabla_segmentos = buscarTablaSeg(direccion->pid);

		// Remuevo el segmento indicado:
		list_remove(tabla_segmentos, id_segmento);

		//TODO: Registro que dicho espacio ahora está libre en memoria
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

		int i = 0;

		int cantidad_frames = tamanio_script/config_FM9.tam_pagina;

		// Creo una tabla de paginas
		list_add(lista_tabla_pag_inv, list_create());

		// TODO: Busco en memoria espacio libre

		// Creo una entrada en la tabla de paginacion invertida en un marco que tenga libre

		for( i = 0; i < cantidad_frames; i = i + 1 ){
			printf("Creo una entrada en el marco %i\n", ultimo_indice);
			//list_add(lista_tabla_pag_inv, crear_fila_tabla_pag_inv(ultimo_indice,pid,i,0)) ;
			ultimo_indice = ultimo_indice+1;
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
	return 0;
}

char* leerMemoriaSeg(int pid, int id_segmento, int offset) {

	t_list *tabla_segmentos = buscarTablaSeg(pid);

	// Obtengo la direccion real/fisica base
	fila_tabla_seg *segmento = list_get(tabla_segmentos, id_segmento);
	int base_segmento = segmento->base_segmento;

	printf("Leo memoria en posición %i: '%s'\n", base_segmento+offset, memoria[base_segmento+offset]);

	return memoria[base_segmento+offset];


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



int cargarEnMemoriaSeg(int pid, int id_segmento, int offset, char* linea) {

	t_list *tabla_segmentos = buscarTablaSeg(pid);

	// Obtengo la direccion real/fisica base
	fila_tabla_seg *segmento = list_get(tabla_segmentos, id_segmento);
	int base_segmento = segmento->base_segmento;

	if (segmento->limite_segmento < offset) {
		return -1;
	}

	memoria[base_segmento+offset] = linea;
	printf("Escribí en posición %i: '%s'\n", base_segmento+offset, memoria[base_segmento+offset]);
	return 0;
}

t_list* buscarTablaSeg(int pid) {
	// Obtengo la tabla de segmentos para ese PID
	int es_pid_buscado(fila_tabla_segmentos_pid *p) {
		return (p->id_proceso == pid);
	}
	fila_tabla_segmentos_pid *relacion_pid_tabla = list_find(tabla_segmentos_pid, (void*) es_pid_buscado);
	t_list* tabla_segmentos = list_get(lista_tablas_segmentos, relacion_pid_tabla->id_tabla_segmentos);
	return tabla_segmentos;
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


int crearTablaPagInv(){

	int tamanio_memoria = config_FM9.tamanio;
	int tamanio_pagina = config_FM9.tam_pagina;
	int i;

	int cantidad_marcos = tamanio_memoria/tamanio_pagina;

	for( i = 0; i < cantidad_marcos; i = i + 1 ){
		crear_fila_tabla_pag_inv(i,0,0,0);
	}


}

int estaVacio(fila_pag_invertida *p){
	return (p->flag == 0);
}
















