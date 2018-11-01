/*
 * MDJ.h
 *
 *  Created on: 19 sep. 2018
 *      Author: utnso
 */

#ifndef MDJ_H_
#define MDJ_H_
#include <commons/string.h>
#include <netdb.h> // Para getaddrinfo
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <stdio.h>
#include <sys/stat.h>
#include <ctype.h>
#include <commons/config.h>
#include <commons/collections/list.h>
#include <openssl/md5.h>
#include <pthread.h>
#include <commons/log.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "../../utils/bibliotecaDeSockets.h"


char* server_ip;
int server_puerto;
int cerrarMDJ;

int tamanio,max_linea,tam_pagina;
int socket_planificador;
int socketFD;
t_log * logger;
float retardo;
char *ip;
char *puerto;
char *dir_actual;
char *string_actual;
t_config* file_MDJ;
t_log* log_MDJ;


t_config *mdj_config;
in_port_t get_in_port(struct sockaddr *sa);

int iniciar_recibirDatos();
void consola();
void configure_logger();
void exit_gracefully(int return_nr);
void leerArchivoConfiguracion(t_config * archivoConfig);
void cmd_cat(char *linea);
void cmd_cd(char *linea);
void cmd_ls(char *linea);
void cmd_pwd();
void cmd_bloque();
void determinarOperacion(int operacion,int fd);
int mySocketM;
int DAM_fd;
int cmd_md5(char *linea);
int string_file(char *pathfile,char **contenido);
struct addrinfo *server_info;

typedef struct{
 	int puerto_mdj;
	char* mount_point;
	int time_delay;
 }t_config_MDJ;

 t_config_MDJ config_MDJ;

int crear_carpetas();
int mkdir_p(const char *path);
void guardarDatos(char *path, int size, int offset, char *buffer) ;
void crearArchivo(char *path, int numero_lineas) ;


#endif /* MDJ_H_ */
