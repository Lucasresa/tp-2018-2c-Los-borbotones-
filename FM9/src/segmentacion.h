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
#include <commons/bitarray.h>

// Creo estructuras de segmentación
t_list *tablas_segmentos_pid;
t_bitarray *memoria_ocupada;

//Fila de la tabla de segmentacion
typedef struct fila_tabla_seg{

	int id_segmento;
	int limite_segmento;
	int base_segmento;

}fila_tabla_seg;

typedef struct fila_tablas_segmentos_pid{

	int id_proceso;
	t_list* tabla_segmentos;

}fila_tablas_segmentos_pid;

void iniciarMemoriaSEG();

int recibirPeticionSeg(int);

void *consolaThreadSeg(void *vargp);

// Utils
int cargarEnMemoriaSeg(int, int, int, char*);
char* leerMemoriaSeg(int, int, int);

t_list* buscarTablaSeg(int pid);
fila_tabla_seg* buscarSegmento(int pid, int id_segmento);

void borrarTablaSeg(int pid);
int borrarSegmento(int pid, int id_segmento);

struct fila_tabla_seg* crear_fila_tabla_seg(int id_segmento, int limite_segmento, int base_segmento);
struct fila_tablas_segmentos_pid* crear_fila_tablas_segmentos_pid(int id_proceso);

int siguiente_id_segmento(t_list* tabla_segmentos);
int segmentoFirstFit(int tamanio);

void printPID(int pid);

#endif /* SEGMENTACION_H_ */
