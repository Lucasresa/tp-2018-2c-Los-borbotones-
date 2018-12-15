/*
 * seg_paginada.h
 *
 *  Created on: 14 dic. 2018
 *      Author: utnso
 */

#ifndef SEG_PAGINADA_H_
#define SEG_PAGINADA_H_
#include "../../biblioteca/bibliotecaDeSockets.h"
#include <readline/readline.h>
#include <commons/string.h>
#include <stdlib.h>
#include <pthread.h>
#include <commons/log.h>
#include <commons/config.h>
#include <commons/collections/list.h>

t_list *tabla_memoria_libre_sp;
t_list *lista_procesos_sp;

typedef struct fila_memoria_libre_sp{
	int indice;
	int flag_presencia;
}fila_memoria_libre_sp;

typedef struct fila_lista_procesos_sp{
	int pid;
	t_list* tabla_de_segmentos;
}fila_lista_procesos_sp;

typedef struct fila_tabla_segmentos_sp{
	int id_segmento;
	int tamanio;
	t_list* tabla_de_paginas;
}fila_tabla_segmentos_sp;

typedef struct fila_tabla_paginas_sp{
	int pagina;
	int base_fisica;
}fila_tabla_paginas_sp;

void inicializarMemoriaSP();
int recibirPeticionSP(int socket);

struct fila_memoria_libre_sp *crear_fila_memoria_libre_sp(int indice, int flag_presencia);
struct fila_lista_procesos_sp *crear_fila_lista_procesos_sp(int pid);
struct fila_tabla_segmentos_sp *crear_fila_tabla_segmentos_sp(int id_segmento, int tamanio);
struct fila_tabla_paginas_sp *crear_fila_tabla_paginas_sp(int pagina, int base_fisica);

struct fila_memoria_libre_sp* asignarFrameVacioSP();
int verificarEspacio(int size_scriptorio);

fila_lista_procesos_sp* buscarProcesoSP(int pid);
fila_tabla_segmentos_sp* buscarSegmentoSP(t_list* tabla_de_segmentos, int id_segmento);
fila_tabla_paginas_sp* buscarPagina(t_list* tabla_de_paginas, int num_pagina);
int siguiente_id_segmento_SP(t_list* tabla_segmentos);

int cargarEstructurasArchivo(fila_lista_procesos_sp* proceso, int tamanio_script);
int cargarEnMemoriaSP(int pid, int id_segmento, int offset, char* linea);

void *consolaThreadSP(void *vargp);

#endif /* SEG_PAGINADA_H_ */
