#include "../../utils/bibliotecaDeSockets.h"
#include <readline/readline.h>
#include <readline/history.h>

#ifndef PROCESS_MDJ_CONSOLA_MDJ_H_
#define PROCESS_MDJ_CONSOLA_MDJ_H_

typedef struct{
 	int puerto_mdj;
	char* mount_point;
	int time_delay;
 }t_config_MDJ;


void consola_MDJ(void);
char** parsearLinea(char* linea);
int identificarProceso(char ** linea_parseada);
void ejecutarComando(int nro_op , char * args);

t_config_MDJ config_MDJ;


#endif /* PROCESS_MDJ_CONSOLA_MDJ_H_ */
