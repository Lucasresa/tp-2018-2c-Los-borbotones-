#include "../../utils/bibliotecaDeSockets.h"
#include <pthread.h>
#include <commons/collections/queue.h>
#include <commons/config.h>

#ifndef S_AFA_H_
#define S_AFA_H_

typedef struct{

	int id;
	char* escriptorio;
	int pc;
	int f_inicializacion;
	char** archivos;

}t_DTB;

typedef struct{

	int port;
	char* algoritmo;
	int quantum;
	int multiprog;
	int retardo;

}configuracion;

t_queue* cola_new;
t_queue* cola_ready;
t_queue* cola_block;
t_queue* cola_exec;
t_queue* cola_exit;
t_config* file;
configuracion config;
int cont_id=0;


#endif /* S_AFA_H_ */
