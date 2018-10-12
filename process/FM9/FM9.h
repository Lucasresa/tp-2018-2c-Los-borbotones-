#ifndef FM9_H_
#define FM9_H_

#include "../../utils/bibliotecaDeSockets.h"
#include <readline/readline.h>
#include <commons/string.h>
#include <stdlib.h>
#include <pthread.h>
#include <commons/log.h>
#include <commons/config.h>

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

void *consolaThread(void*);

void funcionHandshake(int, void*);
void funcionRecibirPeticion(int, void*);

t_modo detectarModo(char*);

#endif /* FM9_H_ */
