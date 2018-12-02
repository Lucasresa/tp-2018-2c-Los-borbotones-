#ifndef PROCESS_S_AFA_LOAD_CONFIG_H_
#define PROCESS_S_AFA_LOAD_CONFIG_H_

#include "gestor_GDT/gestor_GDT.h"
#include <commons/config.h>
#include <sys/inotify.h>
#include <pthread.h>

#define EVENT_SIZE  ( sizeof (struct inotify_event) + 24 )
#define BUF_LEN     ( 1024 * EVENT_SIZE )


void crear_colas(void);
void iniciar_semaforos();
void load_config(void);
t_algoritmo detectarAlgoritmo(char*algoritmo);

void actualizar_file_config(void);
void actualizarColas(int);

#endif /* PROCESS_S_AFA_LOAD_CONFIG_H_ */
