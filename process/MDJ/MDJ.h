/*
 * MDJ.h
 *
 *  Created on: 2 sep. 2018
 *      Author: utnso
 */

#ifndef MDJ_H_
#define MDJ_H_

#include <commons/config.h>

typedef struct{
 	int puerto_mdj;
	char* mount_point;
	int time_delay;
 }t_config_MDJ;

 t_config* file_MDJ;
 t_config_MDJ config_MDJ;

#endif /* MDJ_H_ */
