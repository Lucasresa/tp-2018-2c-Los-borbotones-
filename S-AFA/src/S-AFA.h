#ifndef S_AFA_H_
#define S_AFA_H_

#include <pthread.h>
#include <commons/collections/queue.h>
#include "gestor_GDT/gestor_GDT.h"
#include "load_config.h"

pthread_mutex_t bloqueo_CPU=PTHREAD_MUTEX_INITIALIZER;

void eliminarSocketCPU(int);
void atenderDAM(int*);
void atenderCPU(int*);

int wait_sem(t_semaforo*, int, char*);
int signal_sem(t_semaforo*,char*);

#endif /* S_AFA_H_ */
