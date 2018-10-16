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
#include <stdio.h> // Para manejar archivos
#include <stdlib.h> // Para manejar archivos
#include <sys/stat.h> // Para usar la función mkdir
#include <string.h>
#include <sys/stat.h>   /* mkdir(2) */
#include <errno.h>

 t_config* file_MDJ;
 t_log* log_MDJ;

void determinarOperacion(int,int);
void crearArchivo(char *path, int numero_lineas);
void guardarDatos(char *path, int size, int offset, char *buffer);
int crear_carpetas();
int mkdir_p(const char *path);

#endif /* MDJ_H_ */
