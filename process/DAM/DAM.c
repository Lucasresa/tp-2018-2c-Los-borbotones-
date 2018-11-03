#include "DAM.h"

int main(){

	log_DAM = log_create("DAM.log","DAM",true,LOG_LEVEL_INFO);

	file_DAM = config_create("CONFIG_DAM.cfg");

	config_DAM.puerto_dam=config_get_int_value(file_DAM,"PUERTO");
	config_DAM.ip_safa=string_duplicate(config_get_string_value(file_DAM,"IP_SAFA"));
	config_DAM.puerto_safa=config_get_int_value(file_DAM,"PUERTO_SAFA");
	config_DAM.ip_mdj=string_duplicate(config_get_string_value(file_DAM,"IP_MDJ"));
	config_DAM.puerto_mdj=config_get_int_value(file_DAM,"PUERTO_MDJ");
	config_DAM.ip_fm9=string_duplicate(config_get_string_value(file_DAM,"IP_FM9"));
	config_DAM.puerto_fm9=config_get_int_value(file_DAM,"PUERTO_FM9");
	config_DAM.transfer_size=config_get_int_value(file_DAM,"TRANSFER_SIZE");

	config_destroy(file_DAM);
	int SAFA_fd, MDJ_fd, FM9_fd, rafaga;
	crearSocket(&SAFA_fd);
	crearSocket(&MDJ_fd);
	crearSocket(&FM9_fd);
	//El DAM se conecta con FM9_fd

	if(conectar(&FM9_fd,config_DAM.puerto_fm9,config_DAM.ip_fm9)!=0){
		log_error(log_DAM,"Error al conectarse con FM9");
		exit(1);
	} else {
		log_info(log_DAM, "Conexión con FM9 establecido");
	}

	// Espero para recibir un BUFFER a enviar
/*
	char buffer[] = "abrir racing.txt\nflush loQueSea\nsave otraCosa";
    char* linea = NULL;

	linea = strtok(buffer, "\n");

    while (linea != NULL)
    {
    	log_info(log_DAM,"Enviando archivo al FM9...");
        serializarYEnviar(FM9_fd,ENVIAR_ARCHIVO,linea);
        printf("%s\n", linea);
        linea = strtok(NULL, "\n");
    }
    while (true){
    	sleep(1);
    }
*/
	//El DAM se conecta con MDJ
	if(conectar(&MDJ_fd,config_DAM.puerto_mdj,config_DAM.ip_mdj)!=0){
		log_error(log_DAM,"Error al conectarse con MDJ");
		exit(1);
	}
	else{
		log_info(log_DAM,"Conexion con MDJ establecida");
	}

	//Espero para recibir un DTB a ejecutar

	peticion_guardar guardado = {.path="users/luquitas.txt",.offset=1,.size=20,.buffer="holaholahola"};
	peticion_obtener obtener = {.path="/scripts/checkpoint.escriptorio",.offset=0,.size=20};
	peticion_borrar borrar = {.path="/scripts/test"};
	log_info(log_DAM,"Enviando peticion al MDJ...");

	sleep(2);


	serializarYEnviar(MDJ_fd,BORRAR_ARCHIVO,&borrar);
	serializarYEnviar(MDJ_fd,OBTENER_DATOS,&obtener);
	serializarYEnviar(MDJ_fd,GUARDAR_DATOS,&guardado);
	log_info(log_DAM,"Se envio una peticion de guardado al MDJ");



	return 0;
}
