#ifndef FM9_H_
#define FM9_H_

#include "../../biblioteca/bibliotecaDeSockets.h"
#include "segmentacion.h"
#include <math.h>


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

//Fila de la tabla de paginacion invertida
typedef struct fila_pag_invertida{

	int indice;
	int pid;
	int pagina;
	int flag;

}fila_pag_invertida;


// Esta es la tabla de paginas entera
t_list *lista_tabla_pag_inv;

typedef struct fila_tabla_archivos{
	int archivo;
	int tamanio;
	t_list* paginas_asociadas;

}fila_tabla_archivos;

t_list *tabla_archivos;


int ultimo_indice;

fd_set set_fd;

char** iniciar_memoria();

// Paginacion invertida
struct fila_pag_invertida* crear_fila_tabla_pag_inv(int indice, int pid, int pagina, int flag);

void *consolaThread(void*);

void *funcionHandshake(int, void*);
void *recibirPeticion(int, void*);

t_modo detectarModo(char*);
void crearTablaPagInv();

int recibirPeticionPagInv(int socket);
int traducirOffset(int offset);
int traducirPagina(int pagina,int offset);
int minPagina(int pid);
int cargarEnMemoriaPagInv(int pid, int pagina, int offset, char* linea,int flag);
int cargarEstructuraArchivo(iniciar_scriptorio_memoria* datos_script);
fila_pag_invertida* encontrarFilaVacia();
int encontrarIdDisponible();

char* leerMemoriaPagInv(int pid, int pagina, int offset);

void imprimirTabla();

void *consolaThreadPagInv(void *vargp);

#endif /* FM9_H_ */
