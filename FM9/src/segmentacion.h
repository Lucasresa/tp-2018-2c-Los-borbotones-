/*
 * segmentacion.h
 *
 *  Created on: 3 dic. 2018
 *      Author: utnso
 */

#ifndef SEGMENTACION_H_
#define SEGMENTACION_H_

#include "../../biblioteca/bibliotecaDeSockets.h"
#include <readline/readline.h>
#include <commons/string.h>
#include <stdlib.h>
#include <pthread.h>
#include <commons/log.h>
#include <commons/config.h>
#include <commons/collections/list.h>

// Creo estructuras de segmentación
t_list *lista_tablas_segmentos;
t_list *tabla_segmentos_pid;
t_list *memoria_vacia_seg;

//Fila de la tabla de segmentacion
typedef struct fila_memoria_vacia_seg{

	int base;
	int cant_lineas;

}fila_memoria_vacia_seg;

typedef struct fila_tabla_seg{

	int id_segmento;
	int limite_segmento;
	int base_segmento;

}fila_tabla_seg;

typedef struct fila_tabla_segmentos_pid{

	int id_proceso;
	int id_tabla_segmentos;

}fila_tabla_segmentos_pid;

int recibirPeticionSeg(int);

void *consolaThreadSeg(void *vargp);

// Utils
int cargarEnMemoriaSeg(int, int, int, char*);
char* leerMemoriaSeg(int, int, int);
t_list* buscarTablaSeg(int pid);
t_list* buscarYBorrarTablaSeg(int pid);
struct fila_memoria_vacia_seg *crear_fila_mem_vacia_seg(int base, int cant_lineas);
struct fila_tabla_seg* crear_fila_tabla_seg(int id_segmento, int limite_segmento, int base_segmento);
int segmentoFirstFit(int tamanio);
int siguiente_id_segmento(t_list* tabla_segmentos);
fila_tabla_seg* buscarSegmento(int pid, int id_segmento);

#endif /* SEGMENTACION_H_ */
