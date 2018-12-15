#include "seg_paginada.h"
#include "FM9.h"

int recibirPeticionSP(int socket) {
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
		log_info(log_FM9, "Cargo estructuras de datos para PID nuevo");
		int mensaje;

		iniciar_scriptorio_memoria* datos_script = recibirYDeserializar(socket,header);
		int size_scriptorio = datos_script->size_script;
		int pid = datos_script->pid;

		// Verifico si hay suficiente espacio libre
		if ( verificarEspacio(size_scriptorio) == -1 ) {
			log_error(log_FM9, "Espacio insuficiente");
			mensaje=ERROR_FM9_SIN_ESPACIO;
		} else {
			// Agrego al proceso a mi lista de procesos
			fila_lista_procesos_sp* nuevo_proceso = crear_fila_lista_procesos_sp(pid);
			list_add(lista_procesos_sp, nuevo_proceso);

			cargarEstructurasArchivo(nuevo_proceso, size_scriptorio);

			mensaje=MEMORIA_INICIALIZADA;
		}

		free(datos_script);
		serializarYEnviarEntero(socket,&mensaje);

		return 0;
	}
	case CERRAR_PID:
	{
		log_info(log_FM9, "Recibo instrucción para cerrar PID");
		int pid = *recibirYDeserializarEntero(socket);

		// Busco la tabla de segmentos del proceso
		t_list* tabla_segmentos = buscarYBorrarTablaSeg(pid);

		// Recorro la tabla de segmentos, liberando la memoria
	    void _iterate_elements(fila_tabla_seg *p) {
	    	list_add(memoria_vacia_seg, crear_fila_mem_vacia_seg(p->base_segmento, p->limite_segmento));
	    	free(p);
	    }
	    list_iterate(tabla_segmentos, (void*) _iterate_elements);
	    list_destroy(tabla_segmentos);

	    log_info(log_FM9, "PID cerrado");

		int success=CERRAR_PID;
		serializarYEnviarEntero(socket,&success);
		return 0;
	}
	case ABRIR_ARCHIVO:
	{
		int mensaje;
		log_info(log_FM9, "Recibo instrucción para cargar archivo");
		// Me llega PID y tamaño del archivo (en lineas)
		iniciar_scriptorio_memoria* datos_archivo = recibirYDeserializar(socket,header);

		int size_archivo = datos_archivo->size_script;

		// Verifico si hay suficiente espacio libre
		if ( verificarEspacio(size_archivo) == -1 ) {
			log_error(log_FM9, "Espacio insuficiente");
			mensaje=ERROR_FM9_SIN_ESPACIO;
			serializarYEnviarEntero(socket,&mensaje);
		} else {
			// Busco el proceso en mi lista de procesos
			fila_lista_procesos_sp* proceso = buscarProcesoSP(datos_archivo->pid);

			int id_nuevo_segmento = cargarEstructurasArchivo(proceso, size_archivo);

			mensaje=MEMORIA_INICIALIZADA;
			// Envio la info para acceder al archivo al DAM
			direccion_logica info_acceso_archivo = {.pid=datos_archivo->pid, .base=id_nuevo_segmento,.offset=0};
			serializarYEnviar(socket,ESTRUCTURAS_CARGADAS,&info_acceso_archivo);
		}

		free(datos_archivo);
		return 0;
	}
	case PEDIR_LINEA:
	{
		direccion_logica* direccion;
		direccion = recibirYDeserializar(socket,header);
		char* linea = leerMemoriaSeg(direccion->pid, direccion->base, direccion->offset);
		free(direccion);
		serializarYEnviarString(socket,linea);
		return 0;
	}
	case ESCRIBIR_LINEA:
	{
		int message;
		cargar_en_memoria* info_a_cargar;
		info_a_cargar = recibirYDeserializar(socket,header);
		if (cargarEnMemoriaSP(info_a_cargar->pid, info_a_cargar->id_segmento, info_a_cargar->offset, info_a_cargar->linea)<0) {
			log_error(log_FM9, "Se intentó cargar una línea fuera del limite del segmento.");
			message=ERROR_ESCRIBIR_LINEA;
		} else {
			message=LINEA_CARGADA;
		}
		free(info_a_cargar);

		serializarYEnviarEntero(socket,&message);
		return 0;
	}
	case CERRAR_ARCHIVO:
	{
		log_info(log_FM9, "Recibo instrucción para cerrar archivo");
		//direccion_logica* direccion;
		//direccion = recibirYDeserializar(socket,header);
		//int id_segmento = direccion->base;
		int success;

		serializarYEnviarEntero(socket,&success);
		return 0;
	}

	case LEER_ARCHIVO:
	{
		// Recibo PID y base logica (id de segmento)
		direccion_logica* direccion;
		direccion = recibirYDeserializar(socket,header);
		log_info(log_FM9, "Recibo instrucción para leer archivo (flush) del pid %i, segmento %i", direccion->pid, direccion->base);

		// Obtengo la direccion real
		fila_tabla_seg* segmento_archivo = buscarSegmento(direccion->pid, direccion->base);

		log_info(log_FM9, "Segmento %i tiene base real %i", direccion->base, segmento_archivo->base_segmento);

		char* linea;

		// Envío línea a línea el archivo
		for (int count = 0; count < segmento_archivo->limite_segmento; ++count) {
			linea = leerMemoriaSeg(direccion->pid, direccion->base, count);
			serializarYEnviarString(socket,linea);
			log_info(log_FM9, "Envio línea: %s", linea);
		}
		char finArchivo[] = "FIN_ARCHIVO";
		serializarYEnviarString(socket,finArchivo);
		free(direccion);
		log_info(log_FM9, "Fin leer archivo", linea);
		return 0;
	}
	}
	return 0;
}

struct fila_memoria_libre_sp *crear_fila_memoria_libre_sp(int indice, int flag_presencia) {
	   struct fila_memoria_libre_sp *p;
	   p = (struct fila_memoria_libre_sp *) malloc(sizeof(struct fila_memoria_libre_sp));
	   p->indice = indice;
	   p->flag_presencia = flag_presencia;
/*
	   if (flag_presencia==false) {
		   // Limpio la memoria que acabo de reservar para que no contenga basura
		   int lineas_en_pag = config_FM9.tam_pagina/config_FM9.max_linea;
		   for (int count = 0; count < lineas_en_pag; ++count) {
			   strcpy(memoria[indice*lineas_en_pag+count],"");
		   }
	   }
	   */
	   return p;
}

struct fila_lista_procesos_sp *crear_fila_lista_procesos_sp(int pid) {
	   struct fila_lista_procesos_sp *p;
	   p = (struct fila_lista_procesos_sp *) malloc(sizeof(struct fila_lista_procesos_sp));
	   p->pid = pid;
	   p->tabla_de_segmentos = list_create();
	   return p;
}

struct fila_tabla_segmentos_sp *crear_fila_tabla_segmentos_sp(int id_segmento, int tamanio) {
	   struct fila_tabla_segmentos_sp *p;
	   p = (struct fila_tabla_segmentos_sp *) malloc(sizeof(struct fila_tabla_segmentos_sp));
	   p->id_segmento = id_segmento;
	   p->tamanio = tamanio;
	   p->tabla_de_paginas = list_create();
	   return p;
}

struct fila_tabla_paginas_sp *crear_fila_tabla_paginas_sp(int pagina, int base_fisica) {
	   struct fila_tabla_paginas_sp *p;
	   p = (struct fila_tabla_paginas_sp *) malloc(sizeof(struct fila_tabla_paginas_sp));
	   p->pagina = pagina;
	   p->base_fisica = base_fisica;
	   return p;
}

void inicializarMemoriaSP() {
	tabla_memoria_libre_sp = list_create();
	lista_procesos_sp = list_create();

	int cantidad_marcos = config_FM9.tamanio/config_FM9.tam_pagina;
	fila_memoria_libre_sp* mi_fila;
	for( int i = 0; i < cantidad_marcos; i = i + 1 ){
		mi_fila = crear_fila_memoria_libre_sp(i,false);
		list_add(tabla_memoria_libre_sp,mi_fila);
	}
}

struct fila_memoria_libre_sp* asignarFrameVacioSP() {
	//Creamos la condicion de que el flag esté seteado en 0 (sin usar)
	int estaVacio(fila_memoria_libre_sp *p){
		return (p->flag_presencia == 0);
	}
	//Que frame está sin usar?
	fila_memoria_libre_sp* frame_vacio = list_find(tabla_memoria_libre_sp, (void*) estaVacio);
	frame_vacio->flag_presencia = 1;
	return frame_vacio;
}

int cargarEnMemoriaSP(int pid, int id_segmento, int offset, char* linea) {

	// Obtengo el segmento
	fila_lista_procesos_sp* proceso = buscarProcesoSP(pid);
	fila_tabla_segmentos_sp* segmento = buscarSegmentoSP(proceso->tabla_de_segmentos,id_segmento);

	if (segmento->tamanio < offset) {
		return -1;
	}

	// Calculo la base física a la que tengo que acceder
	int cant_lineas_pag = config_FM9.tam_pagina/config_FM9.max_linea;
	int num_pagina = offset/cant_lineas_pag;

	fila_tabla_paginas_sp* pagina = buscarPagina(segmento->tabla_de_paginas, num_pagina);

	// Calculo el offset de la página al que tengo que acceder
	offset = offset - num_pagina*cant_lineas_pag;

	strcpy(memoria[pagina->base_fisica+offset],linea);
	log_info(log_FM9,"Escribí en posición %i: '%s'", pagina->base_fisica+offset, memoria[pagina->base_fisica+offset]);

	return 0;
}

fila_lista_procesos_sp* buscarProcesoSP(int pid) {
	// Obtengo la tabla de segmentos para ese PID
	int es_pid_buscado(fila_lista_procesos_sp *p) {
		return (p->pid == pid);
	}
	fila_lista_procesos_sp* proceso_buscado = list_find(lista_procesos_sp, (void*) es_pid_buscado);
	return proceso_buscado;
}

fila_tabla_segmentos_sp* buscarSegmentoSP(t_list* tabla_de_segmentos, int id_segmento) {
	// Obtengo la tabla de segmentos para ese ID
	int es_segmento_buscado(fila_tabla_segmentos_sp *p) {
		return (p->id_segmento == id_segmento);
	}
	fila_tabla_segmentos_sp* segmento_buscado = list_find(tabla_de_segmentos, (void*) es_segmento_buscado);
	return segmento_buscado;
}

fila_tabla_paginas_sp* buscarPagina(t_list* tabla_de_paginas, int num_pagina) {
	// Obtengo la tabla de paginas para ese num
	int es_pagina_buscada(fila_tabla_paginas_sp *p) {
		return (p->pagina == num_pagina);
	}
	fila_tabla_paginas_sp* pagina_buscada = list_find(tabla_de_paginas, (void*) es_pagina_buscada);
	return pagina_buscada;
}

int cargarEstructurasArchivo(fila_lista_procesos_sp* proceso, int tamanio_script) {

	// Creo un segmento para el script en la tabla de segmentos
	int id_nuevo_segmento = siguiente_id_segmento_SP(proceso->tabla_de_segmentos);
	fila_tabla_segmentos_sp* segmento_script = crear_fila_tabla_segmentos_sp(id_nuevo_segmento,tamanio_script);
	list_add(proceso->tabla_de_segmentos, segmento_script);

	log_info(log_FM9, "Creado segmento %i para el proceso %i", id_nuevo_segmento, proceso->pid);

	// Creo la tabla de paginas para ese segmento y reservo la memoria
	float tamanio = tamanio_script;
	float cant_lineas_pag = config_FM9.tam_pagina/config_FM9.max_linea;
	int frames_necesarios = ceil(tamanio/cant_lineas_pag);

	for( int i = 0; i < frames_necesarios; i = i + 1 ) {
		//Encontramos un marco disponible
		fila_memoria_libre_sp* unFrameDisponible = asignarFrameVacioSP();
		if (unFrameDisponible == NULL ){
			// loggear que hubo un error
			log_error(log_FM9, "FATAL: No se encontró frame vacío a pesar de verificaciones previas de que sí había.");
		}

		//Lo asignamos a la tabla de páginas
		list_add(segmento_script->tabla_de_paginas, crear_fila_tabla_paginas_sp(i, (int)unFrameDisponible->indice*cant_lineas_pag));

		log_info(log_FM9, "Seteo página %i para el pid %i en el marco %i", i, proceso->pid, unFrameDisponible->indice);
	}
	return id_nuevo_segmento;
}

int siguiente_id_segmento_SP(t_list* tabla_segmentos) {
	int max_id = -1;
    void _iterate_elements(fila_tabla_seg *p) {
        if (p->id_segmento > max_id) {
        	max_id = p->id_segmento;
        }
    }

    list_iterate(tabla_segmentos, (void*) _iterate_elements);
    return ++max_id;
}

int verificarEspacio(int tamanio_script) {
	float tamanio = tamanio_script;
	float cant_lineas_pag = config_FM9.tam_pagina/config_FM9.max_linea;
	int marcosNecesarios = ceil(tamanio/cant_lineas_pag);
	int marcosLibres = 0;
	void cuantasLibres(fila_memoria_libre_sp *p) {
		if (p->flag_presencia == false){
			marcosLibres++;
		}
	}

	list_iterate(tabla_memoria_libre_sp, (void*) cuantasLibres);
	if (marcosNecesarios > marcosLibres) {
		return -1;
	} else {
		return 0;
	}
}

void *consolaThreadSP(void *vargp)
{
	while(true) {
		char *line = readline("$ ");

		if (*line==(int)NULL) continue;

		char *command = strtok(line, " ");

		if (strcmp(command, "dump")==0) {
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
			// Busco la tabla de segmentos del proceso
			fila_lista_procesos_sp* proceso = buscarProcesoSP(pid);
			if (proceso == NULL) {
				log_info(log_FM9, "PID %i no está cargado en memoria", pid);
				continue;
			} else {
				log_info(log_FM9, "Estructuras del process id: %i\n", pid);
				// Recorro la tabla de segmentos, imprimiendo cada segmento
				void print_pagina(fila_tabla_paginas_sp *p) {
					log_info(log_FM9, "%i, %i", p->pagina, p->base_fisica);
				}
				void print_segmento(fila_tabla_segmentos_sp *p) {
					log_info(log_FM9, "N° Segmento: %i, Límite: %i", p->id_segmento, p->tamanio*config_FM9.max_linea);
					log_info(log_FM9, "Tabla de páginas del segmento:", pid);
					log_info(log_FM9, "=========================", pid);
					log_info(log_FM9, "N° pagina, Linea Base", pid);
					log_info(log_FM9, "=========================", pid);
					list_iterate(p->tabla_de_paginas, (void*) print_pagina);
					log_info(log_FM9, "=========================", pid);
				}
				list_iterate(proceso->tabla_de_segmentos, (void*) print_segmento);
				log_info(log_FM9, "CONTENIDO MEMORIA:");

				// Imprimo el contenido de la memoria
				int counter = 0;
				int cant_lineas_pag = config_FM9.tam_pagina/config_FM9.max_linea;
				int limite_segmento;
				void print_pagina_contenido(fila_tabla_paginas_sp *p) {
					for (int i = 0; i < cant_lineas_pag; ++i) {
						if (counter<limite_segmento) {
							log_info(log_FM9, "Posición %i: %s", p->base_fisica+i, memoria[p->base_fisica+i]);

						} else {
							return;
						}

						counter++;
					}
				}
				void _print_memoria_segmento(fila_tabla_segmentos_sp *segmento) {
					log_info(log_FM9, "=");
					log_info(log_FM9, "Segmento: %i, lineas: %i", segmento->id_segmento, segmento->tamanio);
					counter = 0;
					limite_segmento = segmento->tamanio;
					list_iterate(segmento->tabla_de_paginas, (void*) print_pagina_contenido);
				}
				list_iterate(proceso->tabla_de_segmentos, (void*) _print_memoria_segmento);
			}
		} else {
			puts("Comando no reconocido.");
		}
	}
}
