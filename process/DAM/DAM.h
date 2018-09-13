/*
 * DAM.h
 *
 *  Created on: 2 sep. 2018
 *      Author: utnso
 */

#ifndef DAM_H_
#define DAM_H_

#include "../../utils/bibliotecaDeSockets.h"
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

#endif /* DAM_H_ */
