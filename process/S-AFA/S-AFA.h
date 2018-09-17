#include "../../utils/bibliotecaDeSockets.h"
#include <pthread.h>
#include <commons/collections/queue.h>
#include <commons/config.h>
#include "gestor_GDT/gestor_GDT.h"

#ifndef S_AFA_H_
#define S_AFA_H_

typedef enum{
	FIFO,
	RR,
	VRR,
	PROPIO
}t_algoritmo;

typedef struct{

	int id;
	char* escriptorio;
	int pc;
	int f_inicializacion;
	char** archivos;

}t_DTB;

typedef struct{

	int port;
	t_algoritmo algoritmo;
	int quantum;
	int multiprog;
	int retardo;

}t_config_SAFA;

t_queue* cola_new;
t_queue* cola_ready;
t_queue* cola_block;
t_queue* cola_exec;
t_queue* cola_exit;
t_config* file_SAFA;
t_config_SAFA config_SAFA;
int cont_id=0;

void agregarDTBANew(char*);
t_algoritmo detectarAlgoritmo(char*);
void* atenderDAM(void*);

#endif /* S_AFA_H_ */
