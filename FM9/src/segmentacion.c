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

			// Creo un segmento en la tabla
			log_info(log_FM9, "Creo un segmento en posicion %i", base_vacia);
			list_add(tabla_segmentos, crear_fila_tabla_seg(0,size_scriptorio,base_vacia));

			mensaje=MEMORIA_INICIALIZADA;
		}

		free(datos_script);
		serializarYEnviarEntero(socket,&mensaje);
		return 0;
	}
	case CERRAR_PID:
	{
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

		int success=CERRAR_PID;
		serializarYEnviarEntero(socket,&success);
		return 0;
	}
	case ABRIR_ARCHIVO:
	{
		// Me llega PID y tamaño del archivo (en lineas)
		iniciar_scriptorio_memoria* datos_archivo = recibirYDeserializar(socket,header);

		// Busco la tabla de segmentos del proceso
		t_list* tabla_segmentos = buscarTablaSeg(datos_archivo->pid);

		// Busco una dirección con suficiente memoria libre
		int memoria_libre = segmentoFirstFit(datos_archivo->size_script);

		if (memoria_libre == -1) {
			int mensaje = ERROR_FM9_SIN_ESPACIO;
			serializarYEnviarEntero(socket,&mensaje);
		} else {
			// Defino el ID de segmento que voy a usar para este archivo
			int id_nuevo_segmento = siguiente_id_segmento(tabla_segmentos);

			// Agrego la nueva entrada en la tabla de segmentos
			list_add(tabla_segmentos, crear_fila_tabla_seg(id_nuevo_segmento,datos_archivo->size_script,memoria_libre));

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
		direccion_logica* direccion;
		direccion = recibirYDeserializar(socket,header);
		int id_segmento = direccion->base;
		int success;

		// Busco la tabla de segmentos del proceso:
		t_list* tabla_segmentos = buscarTablaSeg(direccion->pid);

		// Remuevo el segmento indicado:
		fila_tabla_seg* segmento_borrado = list_remove(tabla_segmentos, id_segmento);

		if (segmento_borrado == NULL) {
			success=ERROR_SEG_MEM;
		} else {
			success=FINAL_CERRAR;
		}

		// Registro el nuevo espacio libre en memoria
		list_add(memoria_vacia_seg, crear_fila_mem_vacia_seg(segmento_borrado->base_segmento, segmento_borrado->limite_segmento));
		free(direccion);

		serializarYEnviarEntero(socket,&success);
		return 0;
	}

	case LEER_ARCHIVO:
	{
		// Recibo PID y base logica (id de segmento)
		direccion_logica* direccion;
		direccion = recibirYDeserializar(socket,header);

		// Obtengo la direccion real
		t_list* tabla_segmentos = buscarTablaSeg(direccion->pid);
		fila_tabla_seg* segmento_archivo = list_get(tabla_segmentos, direccion->base);

		char* linea;

		// Envío línea a línea el archivo
		for (int count = segmento_archivo->base_segmento; count < (segmento_archivo->base_segmento+segmento_archivo->limite_segmento); ++count) {
			linea = leerMemoriaSeg(direccion->pid, direccion->base, direccion->offset);
			serializarYEnviarString(socket,linea);
		}
		char finArchivo[] = "FIN_ARCHIVO";
		serializarYEnviarString(socket,finArchivo);
		free(direccion);

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

struct fila_memoria_vacia_seg *crear_fila_mem_vacia_seg(int base, int tamanio) {
	   struct fila_memoria_vacia_seg *p;
	   p = (struct fila_memoria_vacia_seg *) malloc(sizeof(struct fila_memoria_vacia_seg));
	   p->base = base;
	   p->cant_lineas = tamanio;
	   return p;
}

char* leerMemoriaSeg(int pid, int id_segmento, int offset) {

	t_list *tabla_segmentos = buscarTablaSeg(pid);

	// Obtengo la direccion real/fisica base
	fila_tabla_seg *segmento = list_get(tabla_segmentos, id_segmento);
	int base_segmento = segmento->base_segmento;

	return memoria[base_segmento+offset];


}

int cargarEnMemoriaSeg(int pid, int id_segmento, int offset, char* linea) {

	t_list *tabla_segmentos = buscarTablaSeg(pid);

	// Obtengo la direccion real/fisica base
	fila_tabla_seg *segmento = list_get(tabla_segmentos, id_segmento);
	int base_segmento = segmento->base_segmento;

	if (segmento->limite_segmento < offset) {
		return -1;
	}

	strcpy(memoria[base_segmento+offset],linea);
	log_info(log_FM9,"Escribí en posición %i: '%s'", base_segmento+offset, memoria[base_segmento+offset]);

	return 0;
}

t_list* buscarTablaSeg(int pid) {
	t_list* tabla_segmentos;
	// Obtengo la tabla de segmentos para ese PID
	int es_pid_buscado(fila_tabla_segmentos_pid *p) {
		return (p->id_proceso == pid);
	}
	fila_tabla_segmentos_pid *relacion_pid_tabla = list_find(tabla_segmentos_pid, (void*) es_pid_buscado);
	if(relacion_pid_tabla == NULL) {
		return NULL;
	}
	tabla_segmentos = list_get(lista_tablas_segmentos, relacion_pid_tabla->id_tabla_segmentos);
	return tabla_segmentos;
}

t_list* buscarYBorrarTablaSeg(int pid) {
	// Obtengo la tabla de segmentos para ese PID
	int es_pid_buscado(fila_tabla_segmentos_pid *p) {
		return (p->id_proceso == pid);
	}
	fila_tabla_segmentos_pid *relacion_pid_tabla = list_remove_by_condition(tabla_segmentos_pid, (void*) es_pid_buscado);
	t_list* tabla_segmentos = list_remove(lista_tablas_segmentos, relacion_pid_tabla->id_tabla_segmentos);
	return tabla_segmentos;
}


int segmentoFirstFit(int lineas_segmento) {
	//Buscamos
	int tiene_suficiente_tamanio(fila_memoria_vacia_seg *p) {
		return (p->cant_lineas > lineas_segmento);
	}

	// Busco el first fit y elimino el hueco ya que va a ser usado por el nuevo segmento
	fila_memoria_vacia_seg *fila_memoria_vacia = list_remove_by_condition(memoria_vacia_seg, (void*) tiene_suficiente_tamanio);

	if (fila_memoria_vacia == NULL) {
		return -1;
	}

	// Añado el nuevo hueco reducido
	fila_memoria_vacia_seg* nueva_fila = crear_fila_mem_vacia_seg(fila_memoria_vacia->base + lineas_segmento, fila_memoria_vacia->cant_lineas - lineas_segmento);
	list_add(memoria_vacia_seg, nueva_fila);

	return fila_memoria_vacia->base;
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
				log_info(log_FM9, "PID %i no está cargado en memoria", pid);
				continue;
			} else {
				log_info(log_FM9, "Estructuras del process id: %i\n", pid);
				log_info(log_FM9, "N° Segmento, Base, Límite", pid);
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
		} else {
			puts("Comando no reconozido.");
		}
	}
}
