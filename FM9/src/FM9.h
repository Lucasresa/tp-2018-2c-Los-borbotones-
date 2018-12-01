#ifndef FM9_H_
#define FM9_H_

#include "../../biblioteca/bibliotecaDeSockets.h"
#include <readline/readline.h>
#include <commons/string.h>
#include <stdlib.h>
#include <pthread.h>
#include <commons/log.h>
#include <commons/config.h>
#include <commons/collections/list.h>

typedef enum{
	SEG,
	TPI,
	SPA
}t_modo;

typedef struct{

	int puerto_fm9;
	t_modo modo;
	int tamanio;
	int max_linea;
	int tam_pagina;

}t_config_FM9;

t_config_FM9 config_FM9;
t_config* file_FM9;
t_log* log_FM9;

char** memoria;

// Creo estructuras de segmentación
t_list *lista_tablas_segmentos;
t_list *tabla_segmentos_pid;
t_list *memoria_vacia_seg;

//Fila de la tabla de paginacion invertida
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


//Fila de la tabla de paginacion invertida
typedef struct fila_pag_invertida{

	int indice;
	int pid;
	int pagina;
	int flag;

}fila_pag_invertida;


// Esta es la tabla de paginas entera
t_list *lista_tabla_pag_inv;

int ultimo_indice;

fd_set set_fd;

char** iniciar_memoria();

// Segmentacion
int cargarEnMemoriaSeg(int, int, int, char*);
char* leerMemoriaSeg(int, int, int);
t_list* buscarTablaSeg(int pid);
struct fila_memoria_vacia_seg *crear_fila_mem_vacia_seg(int base, int cant_lineas);
struct fila_tabla_seg* crear_fila_tabla_seg(int id_segmento, int limite_segmento, int base_segmento);
int segmentoFirstFit(int tamanio);

// Paginacion invertida
struct fila_pag_invertida* crear_fila_tabla_pag_inv(int indice, int pid, int pagina, int flag);

void *consolaThread(void*);

void *funcionHandshake(int, void*);
void *recibirPeticion(int, void*);

int recibirPeticionSeg(int);

t_modo detectarModo(char*);

int traducirOffset(int offset);
int traducirPagina(int pagina,int offset);

#endif /* FM9_H_ */
