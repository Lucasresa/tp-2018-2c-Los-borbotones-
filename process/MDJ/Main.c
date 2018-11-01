/*
 * Main.c
 *
 *  Created on: 19 sep. 2018
 *      Author: utnso
 */

#include "MDJ.h"
#include "../../utils/bibliotecaDeSockets.h"
typedef struct addrinfo* AddrInfo;
typedef struct sockaddr_in SockAddrIn;
typedef struct sockaddr* SockAddr;
typedef char* String;
typedef socklen_t Socklen;
typedef int* PunteroInt;


int main(){
	pthread_t  hilo_consola;
	pthread_t  server;
		cerrarMDJ=0;
	configure_logger();
	char* mdj_config_ruta = strdup("CONFIG_MDJ.cfg");
	mdj_config = config_create(mdj_config_ruta);
	struct addrinfo *server_parametro;
    struct sockaddr_in *saddr;
    leerArchivoConfiguracion(mdj_config);
	int MDJ_fd;
	crearSocket(&MDJ_fd);
	setearParaEscuchar(&MDJ_fd, config_MDJ.puerto_mdj);
	log_info(log_MDJ, "Esperando conexion del DAM");

	DAM_fd=aceptarConexion(MDJ_fd);
	if(DAM_fd==-1){
			log_error(log_MDJ,"Error al establecer conexion con el DAM");
			log_destroy(log_MDJ);
			exit(0);
	}

	log_info(log_MDJ,"Conexion establecida con DAM");

	pthread_create(&hilo_consola, NULL ,(void*)consola , NULL);
	iniciar_recibirDatos();
	//pthread_create(&server, NULL ,(void*)iniciar_conexion  , (void*)parametros);
	while(1){
		if(cerrarMDJ ==1)
			break;
	}
	puts("Hola3");
	pthread_detach(hilo_consola);
	pthread_detach(server);
	free(mdj_config);
	free(server_info);
	//free(saddr);
	free(mdj_config_ruta);
	exit_gracefully(0);
}
