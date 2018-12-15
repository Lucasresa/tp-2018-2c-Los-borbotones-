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
		// Creo una tabla de archivos
		tabla_archivos = list_create();
		//Abro la consola
		pthread_create(&thread_id, NULL, consolaThreadPagInv, NULL);
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
	log_info(log_FM9, "Conexión establecida");
	return 0;
}

void* recibirPeticion(int socket, void* argumentos) {

	if (config_FM9.modo == SEG) {
		recibirPeticionSeg(socket);
	} else if (config_FM9.modo == TPI) {
		recibirPeticionPagInv(socket);
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
		int message;
		iniciar_scriptorio_memoria* datos_script = recibirYDeserializar(socket,header);
		int pagina_base = cargarEstructuraArchivo(datos_script);
		if (pagina_base == -1) {
			message=ERROR_FM9_SIN_ESPACIO;
		} else {
			message=MEMORIA_INICIALIZADA;
		}
		serializarYEnviarEntero(socket,&message);
		return 0;
	}
	case ABRIR_ARCHIVO:
	{
		int message;
		iniciar_scriptorio_memoria* datos_archivo = recibirYDeserializar(socket,header);
		int resultado = cargarEstructuraArchivo(datos_archivo);
		if (resultado == -1) {
		message=ERROR_FM9_SIN_ESPACIO;
		} else {
		message=ESTRUCTURAS_CARGADAS;

		// Envio la info para acceder al archivo al DAM
		direccion_logica info_acceso_archivo = {.pid=datos_archivo->pid, .base=resultado,.offset=0};
		serializarYEnviar(socket,message,&info_acceso_archivo);
		return 0;
		}
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
		info_a_cargar = recibirYDeserializar(socket,header);

		int pagina = info_a_cargar->id_segmento;

		//Cargo en memoria con mi pagina y offset traducido
		cargarEnMemoriaPagInv(info_a_cargar->pid, traducirPagina(pagina,info_a_cargar->offset), traducirOffset(info_a_cargar->offset), info_a_cargar->linea, 1);

		free(info_a_cargar);
		int message=LINEA_CARGADA;
		serializarYEnviarEntero(socket,&message);
		return 0;

	}

	case LEER_ARCHIVO:
	{
		// Recibo PID e indice
		direccion_logica* direccion;
		direccion = recibirYDeserializar(socket,header);

		int idArchivo = direccion->base;

		// Obtengo la direccion real
		fila_tabla_archivos* mifila = list_get(tabla_archivos, idArchivo);

		int inicio = list_get (mifila->paginas_asociadas,0);
		int tamanio = list_size (mifila->paginas_asociadas);

		char* linea;

		int indiceReal = traducirPagina(0,direccion->offset);
		int pagina = list_get (mifila->paginas_asociadas,indiceReal);


		// Envío línea a línea el archivo

		for (int count = inicio; count < tamanio; ++count) {
			//El offset siempre va a ser 0 porque ya aplico mi traducción antes y no necesito que sea traducida
			//Por la función leerMemoriaPagInv

			linea = leerMemoriaPagInv(direccion->pid, pagina, 0);
			serializarYEnviarString(socket,linea);
		}
		char finArchivo[] = "FIN_ARCHIVO";
		serializarYEnviarString(socket,finArchivo);
		free(direccion);

		return 0;
		}

	}
}

int cargarEstructuraArchivo(iniciar_scriptorio_memoria* datos_script){

	int pid = datos_script->pid;

	float tamanio_script = datos_script->size_script;

	float cant_lineas_pag = config_FM9.tam_pagina/config_FM9.max_linea;

	int frames_redondeados = ceil(tamanio_script/cant_lineas_pag);

	int i = 0;

	if (hayMemoriaDisponible(frames_redondeados)){


	for( i = 0; i < frames_redondeados; i = i + 1 ) {
		//Encontramos un marco disponible
		fila_pag_invertida* unaFilaDisponible = encontrarFilaVacia();
		if (unaFilaDisponible == NULL ){
			// loggear que hubo un error
			log_error(log_FM9, "No hay fila vacia.");
			// Avisar al DAM del error
			return -1;
		}

		//El PID ya me viene como parametro
		unaFilaDisponible->pid = pid;
		//Encuentro la minima pagina que puedo asignar
		int paginaAutilizar = minPagina(pid);
		unaFilaDisponible->pagina = minPagina(pid);
		//Seteamos el flag a 1 para que sepa que está ocupado
		unaFilaDisponible->flag = 1;


		//RELLENO LA TABLA DE ARCHIVOS CON LOS DATOS DEL STRUCT:
		fila_tabla_archivos *fila;
		//T Encuentro un ID de pagina disponible
		int idArchivo = encontrarIdDisponible();
		fila->archivo = idArchivo;
		// Averiguo el tamaño
		fila->tamanio = (int) tamanio_script;
		// Agrego a mi lista de paginas asociadas tantas paginas
		for (int j = 0; j > frames_redondeados; j = j+1){
			list_add (fila->paginas_asociadas, (void*) paginaAutilizar);
		}

		list_add(lista_tabla_pag_inv,fila);

		/*
		if (i==0) {
			primera_pagina = unaFilaDisponible->pagina;
		}
		log_info(log_FM9, "Seteo página %i para el pid %i en el marco %i", unaFilaDisponible->pagina, unaFilaDisponible->pid, unaFilaDisponible->indice);
	*/

	return idArchivo;
	free(datos_script);
	}
	} else {
		return -1;
	  }
}


int cargarEnMemoriaPagInv(int pid, int pagina, int offset, char* linea,int flag) {

	//Buscamos
	int es_pid_pagina_buscados(fila_pag_invertida *p) {
		return (p->pid == pid)&&(p->pagina == pagina);
	}

	//Que elemento de la lista cumple con ese pid y esa pagina que se pasan por parametro?
	fila_pag_invertida *fila_tabla_pag_inv = list_find(lista_tabla_pag_inv, (void*) es_pid_pagina_buscados);

	int linea_a_escribir = (config_FM9.tam_pagina/config_FM9.max_linea)*fila_tabla_pag_inv->indice+offset;

	strcpy(memoria[linea_a_escribir],linea);
	printf("Escribí en Posición %i: '%s'\n", linea_a_escribir, memoria[linea_a_escribir]);
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

	lista_tabla_pag_inv = list_create();

	int cantidad_marcos = tamanio_memoria/tamanio_pagina;
	fila_pag_invertida* mi_fila;
	for( i = 0; i < cantidad_marcos; i = i + 1 ){
		mi_fila = crear_fila_tabla_pag_inv(i,-1,-1,0);
		list_add(lista_tabla_pag_inv,mi_fila);
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

//Compara las posiciones para saber cual es la primer pagina que falta, solo sirve si está ordenado
int minPagina(int pid) {
	int i = 0;
	fila_pag_invertida* fila;
	int coincideConLaBusqueda(fila_pag_invertida *p) {
	  return (p->pid == pid)&&(p->pagina == i);
	}
	while (true) {
	fila = list_find(lista_tabla_pag_inv, (void*) coincideConLaBusqueda);
		if (fila == NULL){
			return i;
		}
		else {
			i++;
		}
	}
}

void *consolaThreadPagInv(void *vargp)
{
	while(true) {
		char *line = readline("> ");

		if (*line==(int)NULL) continue;

		char *command = strtok(line, " ");

		//Verifica que se pase por parametro el id de un proceso
		if (strcmp(command, "tabla")==0) {
			char *buffer = strtok(0, " ");
			if (buffer==NULL) {
				puts("Por favor inserte un id");
				continue;
			}
			int pid = atoi(buffer);
			/*
			if (pid == NULL) {
			    printf("PID invalid\n");
			    continue;
			}*/

			// Imprimo la tabla de paginas
				log_info(log_FM9, "Tabla de paginas");
				log_info(log_FM9, "Marco, Pagina, PID");
				log_info(log_FM9, "=========================");
				// Recorro la tabla de segmentos, imprimiendo cada segmento
				void print_segmento(fila_pag_invertida *p) {
					log_info(log_FM9, "    %i    %i     %i   ", p->indice, p->pagina, p->pid);
				}
				list_iterate(lista_tabla_pag_inv, (void*) print_segmento);
				log_info(log_FM9, "=========================", pid);
				log_info(log_FM9, "Memoria del process id: %i", pid);
				/*
				void _print_memoria_pid(fila_tabla_seg *segmento_archivo) {
					log_info(log_FM9, "=");
					for (int count = fila_pag_invertida->indice; count < (segmento_archivo->base_segmento+segmento_archivo->limite_segmento); ++count) {
						log_info(log_FM9, "Posición %i: %s", count, memoria[count]);
					}
				}
				list_iterate(lista_tabla_pag_inv, (void*) _print_memoria_segmento);
				*/

		} else {
			puts("Comando no reconocido.");
		}
	}
}

char* leerMemoriaPagInv(int pid, int pagina, int offset) {

	int es_pid_pagina_buscados(fila_pag_invertida *p) {
		return (p->pid == pid)&&(p->pagina == pagina);
	}
	float cant_lineas_pag = config_FM9.tam_pagina/config_FM9.max_linea;

	//Que elemento de la lista cumple con ese pid y esa pagina que se pasan por parametro?

	fila_pag_invertida *marcoEncontrado = list_find(lista_tabla_pag_inv, (void*) es_pid_pagina_buscados);

	int direccionFisica = marcoEncontrado->indice*cant_lineas_pag+offset;

	printf("Leo memoria en posicion %i: '%s'\n", direccionFisica, memoria[direccionFisica]);

	return memoria[direccionFisica];
}

//Imprime toda la tabla de paginas
void imprimirTabla() {
    int i = 0;
    int indiceBuscado(fila_pag_invertida *p) {
      return (p->indice == i);
    }
    for (i=0 ; i > config_FM9.tamanio/config_FM9.tam_pagina ; i++) {
        fila_pag_invertida* filaAImprimir = list_find(lista_tabla_pag_inv, (void*) indiceBuscado);
        log_info(log_FM9, "%i, %i, %i", filaAImprimir->indice, filaAImprimir->pagina, filaAImprimir->pid);;
    }
}


void eliminarPid(int pid){
// Obtengo la tabla de segmentos para ese PID
    void buscarYreemplazarPid(fila_pag_invertida *p) {
        if (p->pid == pid){
            p->pid = -1;
            p->flag = 0;
            p->pagina = -1;
        }
    }
    list_iterate(lista_tabla_pag_inv, (void*) buscarYreemplazarPid);
}

int encontrarIdDisponible(){
	int i = 0;
	fila_tabla_archivos* fila;
	int idCoincide(fila_tabla_archivos *p) {
		  return (p->archivo == i);
	}
	while (true) {
	fila = list_find(tabla_archivos, (void*) idCoincide);
		if (fila == NULL){
			return i;
		} else {
			i++;
		}
	}

}

int hayMemoriaDisponible(int marcosNecesarios){
	int marcosLibres = 0;
	int cuantasLibres(fila_pag_invertida *p) {
		if (p->flag == 0){
			marcosLibres++;
		}

		list_iterate(lista_tabla_pag_inv, (void*) cuantasLibres);
		return marcosNecesarios <= marcosLibres;

		}
}


