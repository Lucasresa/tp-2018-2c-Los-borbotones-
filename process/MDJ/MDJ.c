/*
 * MDJ.c
 *
 *  Created on: 2 sep. 2018
 *      Author: utnso
 */

#include "MDJ.h"

int main(){
	//Creo un Log para MDJ (FileSystem)
    logger = log_create("MDF.log","MDJ",true,LOG_LEVEL_INFO);

	//Levanto archivo de configuracion del MDJ (FileSystem)
	file_MDJ=config_create("CONFIG_MDJ.cfg");

	config_MDJ.puerto_mdj=config_get_int_value(file_MDJ,"PUERTO");
	config_MDJ.mount_point=string_duplicate(config_get_string_value(file_MDJ,"PUNTO_MONTAJE"));
	config_MDJ.time_delay=config_get_int_value(file_MDJ,"RETARDO");

	config_destroy(file_MDJ);

    int MDJ_fd
    crearSocket(&MDJ_fd);

	setearParaEscuchar(&MDJ_fd, config_MDJ.puerto_mdj);

	log_info(logger, "Escuchando conexiones...");
	fd_set fd_set;
	FD_ZERO(&fd_set);
	FD_SET(MDJ_fd, &fd_set);
	while(true) {
		escuchar(MDJ_fd, &fd_set, &funcionHandshake, NULL, &funcionRecibirPeticion, NULL );
		// Probablemente conviene sacar el sleep()
		sleep(1);
	}
}

t_modo detectarModo(char* modo){
	t_modo modo_enum;

	if(string_equals_ignore_case(modo,"TPI")){
		modo_enum=TPI;
	}else if(string_equals_ignore_case(modo,"SPA")){
		modo_enum=SPA;
	}else {
		modo_enum=SEG;
	}
	return modo_enum;
}

void funcionHandshake(int socket, void* argumentos) {
	log_info(logger, "Conexión establecida");
	return;
}

void funcionRecibirPeticion(int socket, void* argumentos) {
	// Acá estaría la lógica para manejar un pedido.
	return;
}
