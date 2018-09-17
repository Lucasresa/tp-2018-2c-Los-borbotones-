#include <readline/readline.h>
#include <readline/history.h>
#include <commons/string.h>
#include <stdlib.h>
#include <commons/log.h>

#ifndef PROCESS_S_AFA_GESTOR_G_DT_GESTOR_G_DT_H_
#define PROCESS_S_AFA_GESTOR_G_DT_GESTOR_G_DT_H_

void* consola(void);
char** parsearLinea(char* linea);
int identificarProceso(char ** linea_parseada);
void ejecutarProceso(int nro_op , char * args);

t_log* log_SAFA;

#endif /* PROCESS_S_AFA_GESTOR_G_DT_GESTOR_G_DT_H_ */
