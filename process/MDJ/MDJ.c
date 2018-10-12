/*
 * MDJ.c
 *
 *  Created on: 2 sep. 2018
 *      Author: utnso
 */

#include "MDJ.h"

int main(){
	//Creo un Log para MDJ (FileSystem)
    log_MDJ = log_create("MDF.log","MDJ",true,LOG_LEVEL_INFO);

	//Levanto archivo de configuracion del MDJ (FileSystem)
	file_MDJ=config_create("CONFIG_MDJ.cfg");

	config_MDJ.puerto_mdj=config_get_int_value(file_MDJ,"PUERTO");
	config_MDJ.mount_point=string_duplicate(config_get_string_value(file_MDJ,"PUNTO_MONTAJE"));
	config_MDJ.time_delay=config_get_int_value(file_MDJ,"RETARDO");

	config_destroy(file_MDJ);

	//-----------------------------------------------------------------------------------------------
	//Ejecuto la consola del MDJ en un hilo aparte


	pthread_t hilo_consola;

	pthread_create(&hilo_consola,NULL,(void*)consola,NULL);

	pthread_detach(hilo_consola);

	log_info(log_MDJ,"Consola en linea");

	//-----------------------------------------------------------------------------------------------
    //Espero que el DAM se conecte y logueo cuando esto ocurra
	int MDJ_fd;

	int tipo_operacion,bytesRecibidos;

	crearSocket(&MDJ_fd);

	setearParaEscuchar(&MDJ_fd, config_MDJ.puerto_mdj);

	log_info(log_MDJ, "Esperando conexion del DAM");

	int DAM_fd=aceptarConexion(MDJ_fd);

	if(DAM_fd==-1){
		log_error(log_MDJ,"Error al establecer conexion con el DAM");
		log_destroy(log_MDJ);
		exit(0);
	}

	log_info(log_MDJ,"Conexion establecida con DAM");

	//Espero a que el DAM mande alguna solicitud y espero que me mande el tipo de operacion a realizar

	while(1){

		pthread_t hilo_operacion;

		bytesRecibidos=recv(DAM_fd,&tipo_operacion,sizeof(int),0);

		if(bytesRecibidos<=0){

			log_error(log_MDJ,"Error al recibir la operacion del DAM");
			log_destroy(log_MDJ);
			exit(1);
		}

		log_info(log_MDJ,"Peticion recibida del DAM, procesando....");

		determinarOperacion(tipo_operacion,DAM_fd);

	}


}

//Esta funcion deberia determinar que operacion quiere ejecutar el DAM y luego tirar un hilo para atenderla
void determinarOperacion(int operacion,int fd){

	switch(operacion){

	case VALIDAR_ARCHIVO:
	{
		peticion_validar* validacion = recibirYDeserializar(fd,operacion);
		printf("Peticion de validar recibida con el path: %s\n",validacion->path);
		break;
	}
	case CREAR_ARCHIVO:

		break;
	case OBTENER_DATOS:

		break;
	case GUARDAR_DATOS:

		break;

	}

}
