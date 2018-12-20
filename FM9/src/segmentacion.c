#include "segmentacion.h"
#include "FM9.h"

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
		int mensaje;

		iniciar_scriptorio_memoria* datos_script = recibirYDeserializar(socket,header);
		int size_scriptorio = datos_script->size_script;

		// Busco en memoria espacio libre
		int base_vacia = segmentoFirstFit(size_scriptorio);

		if (base_vacia==-1) {
			log_error(log_FM9, "Espacio insuficiente");
			mensaje=ERROR_FM9_SIN_ESPACIO;
		} else {
			// Creo un nuevo PID y su tabla de segmentos
			fila_tablas_segmentos_pid* nuevo_pid = crear_fila_tablas_segmentos_pid(datos_script->pid);
			list_add(tablas_segmentos_pid, nuevo_pid);
			// Creo un segmento en la tabla para ese PID
			list_add(nuevo_pid->tabla_segmentos, crear_fila_tabla_seg(0,size_scriptorio,base_vacia));


			log_info(log_FM9, "Segmento creado para PID %i. ID: 0, Base (linea) real:%i, Limite: %i", datos_script->pid, base_vacia, datos_script->size_script);


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

		// Busco y borro la tabla de segmentos del proceso y libero su memoria
		borrarTablaSeg(pid);

	    log_info(log_FM9, "PID cerrado");

		return 0;
	}
	case ABRIR_ARCHIVO:
	{
		log_info(log_FM9, "Recibo instrucción para cargar archivo");
		// Me llega PID y tamaño del archivo (en lineas)
		iniciar_scriptorio_memoria* datos_archivo = recibirYDeserializar(socket,header);

		// Busco la tabla de segmentos del proceso
		t_list* tabla_segmentos = buscarTablaSeg(datos_archivo->pid);
		log_info(log_FM9, "El archivo necesita %i líneas", datos_archivo->size_script);
		// Busco una dirección con suficiente memoria libre
		int memoria_libre = segmentoFirstFit(datos_archivo->size_script);

		if (memoria_libre == -1) {
			int mensaje = ERROR_FM9_SIN_ESPACIO;
			serializarYEnviarEntero(socket,&mensaje);
			log_error(log_FM9, "No hay espacio para el archivo");
		} else {
			// Defino el ID de segmento que voy a usar para este archivo
			int id_nuevo_segmento = siguiente_id_segmento(tabla_segmentos);

			// Agrego la nueva entrada en la tabla de segmentos
			list_add(tabla_segmentos, crear_fila_tabla_seg(id_nuevo_segmento,datos_archivo->size_script,memoria_libre));

			// Envio la info para acceder al archivo al DAM
			direccion_logica info_acceso_archivo = {.pid=datos_archivo->pid, .base=id_nuevo_segmento,.offset=0};
			log_info(log_FM9, "Segmento creado para PID %i. ID: %i, Base (linea) real:%i, Limite: %i", datos_archivo->pid, id_nuevo_segmento, memoria_libre, datos_archivo->size_script);
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
		if (cargarEnMemoriaSeg(info_a_cargar->pid, info_a_cargar->id_segmento, info_a_cargar->offset, info_a_cargar->linea)<0) {
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
		direccion_logica* direccion;
		direccion = recibirYDeserializar(socket,header);
		int id_segmento = direccion->base;
		int success;

		// Borro el segmento y libero su memoria
		if (borrarSegmento(direccion->pid, id_segmento) == -1) {
			success=ERROR_SEG_MEM;
		} else {
			success=FINAL_CERRAR;
		}

		log_info(log_FM9, "Archivo cerrado");

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
		log_info(log_FM9, "Fin leer archivo");
		return 0;
	}
	}
	return 0;
}

struct fila_tabla_seg *crear_fila_tabla_seg(int id_segmento, int limite_segmento, int base_segmento) {
	   struct fila_tabla_seg *p;
	   p = (struct fila_tabla_seg *) malloc(sizeof(struct fila_tabla_seg));
	   p->id_segmento = id_segmento;
	   p->limite_segmento = limite_segmento;
	   p->base_segmento = base_segmento;

	   // Limpio la memoria que acabo de reservar para que no contenga basura
		for (int count = p->base_segmento; count < (p->base_segmento+p->limite_segmento); ++count) {
			strcpy(memoria[count],"");
		}
	   return p;
}

struct fila_tablas_segmentos_pid* crear_fila_tablas_segmentos_pid(int id_proceso) {
	   struct fila_tablas_segmentos_pid *p;
	   p = (struct fila_tablas_segmentos_pid *) malloc(sizeof(struct fila_tablas_segmentos_pid));
	   p->id_proceso = id_proceso;
	   p->tabla_segmentos = list_create();

	   return p;
}

char* leerMemoriaSeg(int pid, int id_segmento, int offset) {

	// Obtengo la direccion real/fisica base
	fila_tabla_seg* segmento = buscarSegmento(pid, id_segmento);
	int base_segmento = segmento->base_segmento;

	if (segmento->limite_segmento < offset) {
		log_error(log_FM9, "Se intentó leer fuera del límite de un segmento");
		return NULL;
	}

	log_info(log_FM9, "Leo posición %i: %s", base_segmento+offset, memoria[base_segmento+offset]);
	return memoria[base_segmento+offset];

}

int cargarEnMemoriaSeg(int pid, int id_segmento, int offset, char* linea) {

	// Obtengo la direccion real/fisica base
	fila_tabla_seg *segmento = buscarSegmento(pid, id_segmento);
	int base_segmento = segmento->base_segmento;

	if (segmento->limite_segmento < offset) {
		return -1;
	}

	strcpy(memoria[base_segmento+offset],linea);
	log_info(log_FM9,"Escribí en posición %i: '%s'", base_segmento+offset, memoria[base_segmento+offset]);

	return 0;
}

t_list* buscarTablaSeg(int pid) {
	// Obtengo la tabla de segmentos para ese PID
	int es_pid_buscado(fila_tablas_segmentos_pid *p) {
		return (p->id_proceso == pid);
	}
	fila_tablas_segmentos_pid *relacion_pid_tabla = list_find(tablas_segmentos_pid, (void*) es_pid_buscado);
	if(relacion_pid_tabla == NULL) {
		return NULL;
	}
	return relacion_pid_tabla->tabla_segmentos;
}

void borrarTablaSeg(int pid) {
	// Obtengo la tabla de segmentos para ese PID
	int es_pid_buscado(fila_tablas_segmentos_pid *p) {
		return (p->id_proceso == pid);
	}
	fila_tablas_segmentos_pid *tabla_segmentos_pid = list_remove_by_condition(tablas_segmentos_pid, (void*) es_pid_buscado);

	// Recorro la tabla de segmentos, liberando la memoria
    void _iterate_elements(fila_tabla_seg *p) {
    	log_info(log_FM9, "Borrando segmento con base en %i",p->base_segmento);

		for (int i = 0; i<p->limite_segmento; i++) {
			bitarray_clean_bit(memoria_ocupada, p->base_segmento+i);
		}
    	free(p);
    }
    list_iterate(tabla_segmentos_pid->tabla_segmentos, (void*) _iterate_elements);
    list_destroy(tabla_segmentos_pid->tabla_segmentos);

	free(tabla_segmentos_pid);
}

int borrarSegmento(int pid, int id_segmento) {
	// Obtengo la tabla de segmentos para ese PID
	t_list* tabla_segmentos = buscarTablaSeg(pid);

	// Obtengo el id de segmento
	int es_seg_id_buscado(fila_tabla_seg *p) {
		return (p->id_segmento == id_segmento);
	}
	fila_tabla_seg* segmento_buscado = list_remove_by_condition(tabla_segmentos, (void*) es_seg_id_buscado);

	if (segmento_buscado == NULL) {
		return -1;
	}

	// Recorro el segmento, liberando la memoria
	for (int i = 0; i<segmento_buscado->limite_segmento; i++) {
		bitarray_clean_bit(memoria_ocupada, segmento_buscado->base_segmento+i);
	}
	free(segmento_buscado);
	return 0;
}


int segmentoFirstFit(int lineas_segmento) {

	int base_libre = -1;
	int tamanio_base = 0;
	int flag_midiendo_base_libre = false;

	// Buscamos una base libre de suficiente tamaño
	for (int j = 0; j<bitarray_get_max_bit(memoria_ocupada); j++) {
		if (flag_midiendo_base_libre) {
			if (bitarray_test_bit(memoria_ocupada, j)) {
				base_libre = -1;
				tamanio_base = 0;
				flag_midiendo_base_libre = false;
				continue;
			} else {
				tamanio_base++;
			}
		} else {
			if (bitarray_test_bit(memoria_ocupada, j)) {
				base_libre = -1;
				continue;
			} else {
				tamanio_base=1;
				flag_midiendo_base_libre = true;
				base_libre = j;
			}
		}


		if (tamanio_base == lineas_segmento) {
			flag_midiendo_base_libre = false;
			break;
		}

	}


	if (flag_midiendo_base_libre == true) {
		base_libre=-1;
	}

	// Marco el nuevo segmento de base libre como memoria ocupada:
	if (base_libre!=-1) {
		for (int i = 0; i<lineas_segmento; i++) {
			bitarray_set_bit(memoria_ocupada, base_libre+i);
		}
	}

	return base_libre;
}

fila_tabla_seg* buscarSegmento(int pid, int id_segmento) {
	t_list* tabla_segmentos = buscarTablaSeg(pid);

	// Obtengo la tabla de segmentos para ese PID
	int es_id_buscado(fila_tabla_seg *p) {
		return (p->id_segmento == id_segmento);
	}
	fila_tabla_seg *segmento = list_find(tabla_segmentos, (void*) es_id_buscado);

	if(segmento == NULL) {
		log_error(log_FM9, "Se intentó acceder a un segmento que no existe en memoria.");
		return NULL;
	}

	return segmento;
}

int siguiente_id_segmento(t_list* tabla_segmentos) {
	int max_id = 0;
    void _iterate_elements(fila_tabla_seg *p) {
        if (p->id_segmento > max_id) {
        	max_id = p->id_segmento;
        }
    }

    list_iterate(tabla_segmentos, (void*) _iterate_elements);
    return ++max_id;
}

void *consolaThreadSeg(void *vargp)
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
			t_list* tabla_segmentos = buscarTablaSeg(pid);
			if (tabla_segmentos == NULL) {
				printf("PID %i no está cargado en memoria\n", pid);
				continue;
			} else {
				printf("Estructuras del process id: %i\n", pid);
				printf("N° Segmento, Linea Base, Límite\n");
				printf("=========================\n");
				// Recorro la tabla de segmentos, imprimiendo cada segmento
				void print_segmento(fila_tabla_seg *p) {
					printf("%i, %i, %i\n", p->id_segmento, p->base_segmento, p->limite_segmento*config_FM9.max_linea);
				}
				list_iterate(tabla_segmentos, (void*) print_segmento);
				printf("=========================\n");
				printf("Memoria del process id: %i\n", pid);
				void _print_memoria_segmento(fila_tabla_seg *segmento_archivo) {
					printf("=\n");
					for (int count = segmento_archivo->base_segmento; count < (segmento_archivo->base_segmento+segmento_archivo->limite_segmento); ++count) {
						printf("Posición %i: %s\n", count, memoria[count]);
					}
				}
				list_iterate(tabla_segmentos, (void*) _print_memoria_segmento);
			}
		} else {
			puts("Comando no reconocido.");
		}
	}
}

void printPID(int pid) {
	// Busco la tabla de segmentos del proceso
	t_list* tabla_segmentos = buscarTablaSeg(pid);
	if (tabla_segmentos == NULL) {
		log_info(log_FM9, "PID %i no está cargado en memoria", pid);
	} else {
		log_info(log_FM9, "Estructuras del process id: %i\n", pid);
		log_info(log_FM9, "N° Segmento, Linea Base, Límite", pid);
		log_info(log_FM9, "=========================", pid);
		// Recorro la tabla de segmentos, imprimiendo cada segmento
		void print_segmento(fila_tabla_seg *p) {
			log_info(log_FM9, "%i, %i, %i", p->id_segmento, p->base_segmento, p->limite_segmento*config_FM9.max_linea);
		}
		list_iterate(tabla_segmentos, (void*) print_segmento);
		log_info(log_FM9, "=========================", pid);
		log_info(log_FM9, "Memoria del process id: %i", pid);
		void _print_memoria_segmento(fila_tabla_seg *segmento_archivo) {
			log_info(log_FM9, "=");
			for (int count = segmento_archivo->base_segmento; count < (segmento_archivo->base_segmento+segmento_archivo->limite_segmento); ++count) {
				log_info(log_FM9, "Posición %i: %s", count, memoria[count]);
			}
		}
		list_iterate(tabla_segmentos, (void*) _print_memoria_segmento);
	}
}

void iniciarMemoriaSEG() {
	tablas_segmentos_pid = list_create();

	// Inicio bitarray de memoria vacía
	int cantidad_lineas = config_FM9.tamanio / config_FM9.max_linea;
	int bitarray_size = cantidad_lineas/8;
	char* data = malloc( sizeof(char) *  (bitarray_size + 1) );
	for (int j = 0; j<bitarray_size; j++) {
		data[j] = 0;
	}
	memoria_ocupada = bitarray_create_with_mode(data, sizeof(data), MSB_FIRST);

}
