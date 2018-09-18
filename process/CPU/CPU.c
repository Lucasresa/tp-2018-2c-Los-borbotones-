#include "CPU.h"
#include "../S-AFA/S-AFA.h"

int main(){

	//Creo un log para el CPU

	log_CPU = log_create("CPU.log","CPU",true,LOG_LEVEL_INFO);

	file_CPU=config_create("CONFIG_CPU.cfg");

	config_CPU.ip_safa=string_duplicate(config_get_string_value(file_CPU,"IP_SAFA"));
	config_CPU.puerto_safa=config_get_int_value(file_CPU,"PUERTO_SAFA");
	config_CPU.ip_diego=string_duplicate(config_get_string_value(file_CPU,"IP_DIEGO"));
	config_CPU.puerto_diego=config_get_int_value(file_CPU,"PUERTO_DIEGO");
	config_CPU.ip_fm9=string_duplicate(config_get_string_value(file_CPU,"IP_FM9"));
	config_CPU.puerto_fm9=config_get_int_value(file_CPU,"PUERTO_FM9");
	config_CPU.retardo=config_get_int_value(file_CPU,"RETARDO");

	config_destroy(file_CPU);
/*
	printf("ip safa: %s\tpuerto safa: %d\tip diego: %s\tpuerto diego: %d\tretardo: %d\n",
				config_CPU.ip_safa,config_CPU.puerto_safa,config_CPU.ip_diego,config_CPU.puerto_diego,config_CPU.retardo);
*/

	int SAFA_fd,DAM_fd,FM9_fd,rafaga;

	t_DTB exec_DTB;

	crearSocket(&SAFA_fd);
	crearSocket(&DAM_fd);
	crearSocket(&FM9_fd);

	//El CPU se conecta con SAFA
	if(conectar(&SAFA_fd,config_CPU.puerto_safa,config_CPU.ip_safa)!=0){
		log_error(log_CPU,"Error al conectarse con SAFA");
		exit(1);
	}

	log_info(log_CPU,"Conexion exitosa con SAFA");

	//Aca hace el handshake con SAFA para saber la rafaga de planificacion
	if(recv(SAFA_fd,(void*)rafaga,sizeof(int),0)<=0){
		log_destroy(log_CPU);
		perror("Error al recibir la rafaga de planificacion");
		exit(2);
	}

	//El CPU se conecta con "el diego"
	if(conectar(&DAM_fd,config_CPU.puerto_diego,config_CPU.ip_diego)!=0){
		log_error(log_CPU,"Error al conectarse con DAM");
		exit(1);
	}

	if(conectar(&FM9_fd,config_CPU.puerto_fm9,config_CPU.ip_fm9)!=0){
		log_error(log_CPU,"Error al conectarse con FM9");
		exit(1);
	}


	//Espero para recibir un DTB a ejecutar




	return 0;
}
