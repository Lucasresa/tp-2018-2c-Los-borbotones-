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
	pthread_t  hilo_conexion_dam;
	cerrarMDJ=0;
	configure_logger();
	char* mdj_config_ruta = strdup("CONFIG_MDJ.cfg");
	mdj_config = config_create(mdj_config_ruta);
	leerArchivoConfiguracion(mdj_config);
	inicializar();

	pthread_create(&hilo_conexion_dam, NULL ,(void*)conexion_dam , NULL);
	pthread_create(&hilo_consola, NULL ,(void*)consola , NULL);

	//pthread_create(&server, NULL ,(void*)iniciar_conexion  , (void*)parametros);
	while(1){
		if(cerrarMDJ ==1)
			break;
	}
	puts("Hola3");
	pthread_detach(hilo_consola);
	pthread_detach(hilo_conexion_dam);
	free(mdj_config);

	free(mdj_config_ruta);
	exit_gracefully(0);
}
