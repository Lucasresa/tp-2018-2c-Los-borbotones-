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

	int SAFA_fd,DAM_fd,FM9_fd;

	crearSocket(&SAFA_fd);
	crearSocket(&DAM_fd);
	crearSocket(&FM9_fd);

	//El CPU se conecta con SAFA
	if(conectar(&SAFA_fd,config_CPU.puerto_safa,config_CPU.ip_safa)!=0){
		log_error(log_CPU,"Error al conectarse con SAFA");
		exit(1);
	}

	log_info(log_CPU,"Conexion exitosa con SAFA");


	//El CPU se conecta con "el diego"
//	if(conectar(&DAM_fd,config_CPU.puerto_diego,config_CPU.ip_diego)!=0){
//		log_error(log_CPU,"Error al conectarse con DAM");
//		exit(1);
//	}
//
//	if(conectar(&FM9_fd,config_CPU.puerto_fm9,config_CPU.ip_fm9)!=0){
//		log_error(log_CPU,"Error al conectarse con FM9");
//		exit(1);
//	}


	while(1){

		//El SAFA me envia la rafaga a ejecutar
		if(recv(SAFA_fd,&rafaga_recibida,sizeof(int),0)<=0){
			log_destroy(log_CPU);
			perror("Error al recibir la rafaga de planificacion");
			exit(2);
		}
		log_info(log_CPU,"Rafaga de CPU recibida: %d",rafaga_recibida);

		//Espero para recibir un DTB a ejecutar

		log_info(log_CPU,"Esperando DTB del S-AFA");

		t_DTB dtb=RecibirYDeserializarDTB(SAFA_fd);

		log_info(log_CPU,"DTB Recibido con ID: %d",dtb.id);

		rafaga_actual=rafaga_recibida;

		if(dtb.f_inicializacion==0){
			log_info(log_CPU,"El DTB tiene su flag en 0, comenzando inicializacion...");
			inicializarDTB(DAM_fd,SAFA_fd,&dtb);
		}else{
			log_info(log_CPU,"El DTB tiene su flag en 1, comenzando con la ejecucion...");
			comenzarEjecucion(DAM_fd,FM9_fd,dtb);
		}
		usleep(config_CPU.retardo*3000);
	}

	return 0;
}

void inicializarDTB(int DAM_fd,int SAFA_fd,t_DTB* dtb){

	void*buffer;
	int protocolo=BLOQUEAR_PROCESO;
	//Enviar peticion de "abrir" al MDJ por medio de "el diego"
	//desbloqueo_dummy dummy = {.path = string_duplicate(dtb->escriptorio), .id_dtb = dtb->id};

	//serializarYEnviar(DAM_fd,DESBLOQUEAR_DUMMY,&dummy);

	//Una vez enviando el dummy a DAM tengo que avisarle a SAFA que bloquee el proceso mientras se carga en memoria el script

	send(SAFA_fd,&protocolo,sizeof(int),0);

	serializarYEnviarEntero(SAFA_fd,&(dtb->id));

}

void comenzarEjecucion(int DAM, int FM9, t_DTB dtb){

	/*
	 * Primero tengo que pedirle al fm9 las lineas para ejecutar
	 * Una vez que recibo las lineas, tengo que parsearlas con mi parser
	 * Luego comunicarme con DAM y enviarle algo dependiendo de la instruccion que se este ejecutando
	 * En caso de ser una I/O debo avisar a SAFA y desalojar el DTB para poder ejecutar otro
	 * Por cada linea que se ejecuta debo decrementar la rafaga_actual, cuando esta llegue a 0 hay que informar a SAFA
	*/
	usleep(config_CPU.retardo*3000);
}

