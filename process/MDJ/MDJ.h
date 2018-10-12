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
#include "consola_MDJ.h"

 t_config* file_MDJ;
 t_log* log_MDJ;

void determinarOperacion(int,int);

#endif /* MDJ_H_ */
