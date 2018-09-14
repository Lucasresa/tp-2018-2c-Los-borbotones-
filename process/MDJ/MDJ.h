/*
 * MDJ.h
 *
 *  Created on: 2 sep. 2018
 *      Author: utnso
 */

#ifndef MDJ_H_
#define MDJ_H_

#include "../../utils/bibliotecaDeSockets.h"
#include <pthread.h>
#include <commons/log.h>
#include <commons/config.h>

typedef struct{
 	int puerto_mdj;
	char* mount_point;
	int time_delay;
 }t_config_MDJ;

 t_config* file_MDJ;
 t_config_MDJ config_MDJ;
 t_log* logger;

void funcionHandshake(int, void*);
void funcionRecibirPeticion(int, void*);

t_modo detectarModo(char*);

#endif /* MDJ_H_ */
