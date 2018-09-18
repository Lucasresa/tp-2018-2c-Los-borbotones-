#include "../../utils/bibliotecaDeSockets.h"
#include <commons/config.h>
#include <commons/log.h>

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

#endif /* CPU_H_ */
