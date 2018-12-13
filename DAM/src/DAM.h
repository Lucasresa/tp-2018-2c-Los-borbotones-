#ifndef DAM_H_
#define DAM_H_

#include "../../biblioteca/bibliotecaDeSockets.h"
#include <pthread.h>
#include <commons/log.h>
#include <commons/config.h>

typedef struct{

	int puerto_dam;
	char* ip_safa;
	int puerto_safa;
	char* ip_mdj;
	int puerto_mdj;
	char* ip_fm9;
	int puerto_fm9;
	int transfer_size;

}t_config_DAM;

t_config* file_DAM;
t_config_DAM config_DAM;
t_log* log_DAM;

fd_set set_fd;
int SAFA_fd, MDJ_fd, FM9_fd; //rafaga;
void* funcionHandshake(int, void*);
void* recibirPeticion(int socket, void* argumentos);

int cargarArchivoFM9(int pid, char* buffer, int* error_holder);
int cargarScriptFM9(int pid, char* buffer, int* error_holder);

int guardarArchivoMDJ(char* path, char* buffer);
char* obtenerArchivoMDJ(char *path);
int validarArchivoMDJ(int , char* );
int crearArchivoMDJ(int ,int ,peticion_crear* );

int recibirHeader(int socket, int headerEsperado);

void escucharSAFA(int* );

pthread_mutex_t mutex_SAFA;

void testeoFM9();
#endif /* DAM_H_ */
