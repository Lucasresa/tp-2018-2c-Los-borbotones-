#include <readline/readline.h>
#include <commons/string.h>
#include <stdlib.h>

#ifndef PROCESS_S_AFA_GESTOR_G_DT_GESTOR_G_DT_H_
#define PROCESS_S_AFA_GESTOR_G_DT_GESTOR_G_DT_H_

void consola(void);
char** parsearLinea(char* linea);
int identificarProceso(char ** linea_parseada);
void ejecutarProceso(int nro_op , char * args);

#endif /* PROCESS_S_AFA_GESTOR_G_DT_GESTOR_G_DT_H_ */
