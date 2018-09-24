#include <readline/readline.h>
#include <readline/history.h>
#include <commons/string.h>
#include <stdlib.h>
#include <commons/log.h>
#include <commons/collections/queue.h>
#include <commons/collections/list.h>
#include <commons/collections/dictionary.h>
#include <stdbool.h>
#include "../../../utils/bibliotecaDeSockets.h"

#ifndef PROCESS_S_AFA_GESTOR_G_DT_GESTOR_G_DT_H_
#define PROCESS_S_AFA_GESTOR_G_DT_GESTOR_G_DT_H_

typedef struct{

	int port;
	t_algoritmo algoritmo;
	int quantum;
	int multiprog;
	int retardo;

}t_config_SAFA;

void* consola(void);
char** parsearLinea(char* linea);
int identificarProceso(char ** linea_parseada);
void ejecutarComando(int nro_op , char * args);

void agregarDTBDummyANew(char*,t_DTB*);
void ejecutarPLP();
void ejecutarPCP();
void ejecutarProceso(t_DTB*,int);

t_queue* cola_new;
t_queue* cola_ready;
t_queue* cola_block;

t_dictionary* cola_exec;

t_queue* cola_exit;

t_config_SAFA config_SAFA;

t_log* log_SAFA;

t_list* CPU_libres;


#endif /* PROCESS_S_AFA_GESTOR_G_DT_GESTOR_G_DT_H_ */
