#ifndef PROCESS_S_AFA_LOAD_CONFIG_H_
#define PROCESS_S_AFA_LOAD_CONFIG_H_

#include "gestor_GDT/gestor_GDT.h"
#include <commons/config.h>

t_config* file_SAFA;

void load_config(void);
t_algoritmo detectarAlgoritmo(char*algoritmo);


#endif /* PROCESS_S_AFA_LOAD_CONFIG_H_ */
