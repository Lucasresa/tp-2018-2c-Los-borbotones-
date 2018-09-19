#include <pthread.h>
#include <commons/collections/queue.h>
#include <commons/config.h>
#include "gestor_GDT/gestor_GDT.h"

#ifndef S_AFA_H_
#define S_AFA_H_

t_config* file_SAFA;

t_algoritmo detectarAlgoritmo(char*);
void atenderDAM(int*);
void atenderCPU(int*);

#endif /* S_AFA_H_ */
