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

typedef struct fila_tabla_seg{

	int id_segmento;
	int limite_segmento;
	int base_segmento;

}fila_tabla_seg;

typedef struct fila_tabla_segmentos_pid{

	int id_proceso;
	int id_tabla_segmentos;

}fila_tabla_segmentos_pid;

t_config_FM9 config_FM9;
t_config* file_FM9;
t_log* log_FM9;

char** memoria;
int mem_libre_base;
// Creo estructuras de segmentación
t_list *lista_tablas_segmentos;
t_list *tabla_segmentos_pid;

//PAGINACION INVERTIDA

typedef struct fila_pag_invertida{

	int indice;
	int pid;
	int pagina;

}fila_pag_invertida;

// Estructuras de paginacion
t_list *lista_tabla_pagina;


fd_set set_fd;

char** iniciar_memoria();
int cargarEnMemoria(int pid, int id_segmento, int offset, char* linea);

struct fila_tabla_seg* crear_fila_tabla_seg(int id_segmento, int limite_segmento, int base_segmento);

void *consolaThread(void*);

void funcionHandshake(int, void*);
void funcionRecibirPeticion(int, void*);

t_modo detectarModo(char*);

#endif /* FM9_H_ */
