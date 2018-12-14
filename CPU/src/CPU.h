#include "../../biblioteca/bibliotecaDeSockets.h"
#include <commons/config.h>
#include <commons/log.h>
#include "Parser/parser.h"
#include <commons/collections/list.h>

#ifndef CPU_H_
#define CPU_H_


typedef struct{
	char* ip_safa;
	int puerto_safa;
	char* ip_diego;
	int puerto_diego;
	char* ip_fm9;
	int puerto_fm9;
	int retardo;
}t_config_CPU;

t_config_CPU config_CPU;
t_config* file_CPU;
t_log* log_CPU;

int rafaga_recibida, rafaga_actual;


void inicializarDTB(int ,int ,t_DTB* );
void comenzarEjecucion(int, int, int ,t_DTB);
void notificarSAFA(int, int, t_DTB);
int isOpenFile(t_DTB , char*);
void actualizarDTB(t_DTB* );
int getAccesoFile(t_DTB, char*);

#endif /* CPU_H_ */
