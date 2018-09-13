#include "FM9.h"

int main(){
	logger = log_create("FM9.log","FM9",true,LOG_LEVEL_INFO);

	file_FM9=config_create("CONFIG_FM9.cfg");

	config_FM9.puerto_fm9=config_get_int_value(file_FM9,"PUERTO");
	config_FM9.modo=detectarModo(config_get_string_value(file_FM9,"MODO"));
	config_FM9.tamanio=config_get_int_value(file_FM9,"TAMANIO");
	config_FM9.max_linea=config_get_int_value(file_FM9,"MAX_LINEA");
	config_FM9.tam_pagina=config_get_int_value(file_FM9,"TAM_PAGINA");

	config_destroy(file_FM9);

	int listener_socket;
	crearSocket(&listener_socket);

	setearParaEscuchar(&listener_socket, config_FM9.puerto_fm9);

	log_info(logger, "Escuchando conexiones...");
	fd_set fd_set;
	FD_ZERO(&fd_set);
	FD_SET(listener_socket, &fd_set);
	while(true) {
		escuchar(listener_socket, &fd_set, &funcionHandshake, NULL, &funcionRecibirPeticion, NULL );
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
