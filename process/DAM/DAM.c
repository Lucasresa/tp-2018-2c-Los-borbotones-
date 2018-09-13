#include "DAM.h"

int main(){

	logger = log_create("DAM.log","DAM",true,LOG_LEVEL_INFO);

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
		log_error(logger,"Error al conectarse con SAFA");
		exit(1);
	} else {
		log_info(logger, "Conexión con FM9 establecido");
	}

	//Espero para recibir un DTB a ejecutar




	return 0;
}
