#ifndef S_AFA_H_
#define S_AFA_H_

#include <commons/collections/queue.h>
#include "gestor_GDT/gestor_GDT.h"
#include "load_config.h"

void eliminarSocketCPU(int);
void atenderDAM(int*);
void atenderCPU(int*);

int wait_sem(t_semaforo*, int, char*);
int signal_sem(t_semaforo*,char*);

int dtb_esperando=0;

#endif /* S_AFA_H_ */
