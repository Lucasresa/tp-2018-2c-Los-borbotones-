#include "S-AFA.h"


int main(){

	log_SAFA=log_create("log_SAFA","SAFA",true,LOG_LEVEL_INFO);

	lista_CPU=list_create();


	//Creo las colas del S-AFA

	cola_new = queue_create();
	cola_ready = queue_create();
	cola_block = queue_create();
	cola_exec = queue_create();
	cola_exit = queue_create();

	//Levanto archivo de configuracion del S-AFA

	file_SAFA=config_create("CONFIG_S-AFA.cfg");

	config_SAFA.port=config_get_int_value(file_SAFA,"PUERTO");
	config_SAFA.algoritmo=detectarAlgoritmo(config_get_string_value(file_SAFA,"ALGORITMO"));
	config_SAFA.quantum=config_get_int_value(file_SAFA,"QUANTUM");
	config_SAFA.multiprog=config_get_int_value(file_SAFA,"MULTIPROGRAMACION");
	config_SAFA.retardo=config_get_int_value(file_SAFA,"RETARDO_PLANIF");

	config_destroy(file_SAFA);

	//-------------------------------------------------------------------------------------------------------

	//Tiro un hilo para ejecutar la consola

	pthread_t hiloConsola;

	pthread_create(&hiloConsola,NULL,(void*)consola,NULL);

	pthread_detach(hiloConsola);

	log_info(log_SAFA,"Consola en linea...");

	//-------------------------------------------------------------------------------------------------------

	//Aca habria que hacer la parte de la conexion con los demas procesos usando sockets. (Usando la shared library de sockets

	int SAFA_fd,DAM_fd,CPU_fd;
	crearSocket(&SAFA_fd);


	setearParaEscuchar(&SAFA_fd,config_SAFA.port);

	log_info(log_SAFA,"Escuchando nuevas conexiones....");

	DAM_fd=aceptarConexion(SAFA_fd);

	if(DAM_fd==-1){
		perror("Error de conexion con DAM");
		log_destroy(log_SAFA);
		exit(1);
	}

	log_info(log_SAFA,"Conexion exitosa con DAM");

	pthread_t hiloDAM;

	pthread_create(&hiloDAM,NULL,(void*)atenderDAM,(void*)&DAM_fd);

	pthread_detach(hiloDAM);


	//El hilo main se queda esperando que se conecten nuevas CPU

	while(1){

		pthread_t hiloCPU;

		if((CPU_fd=aceptarConexion(SAFA_fd))!=-1){

			t_estado_CPU cpu_new={.CPU_fd=CPU_fd,.estado=false};

			pthread_create(&hiloCPU,NULL,(void*)atenderCPU,(void*)&hiloCPU);
			pthread_detach(hiloCPU);
			log_info(log_SAFA,"Conexion exitosa con la CPU %d",CPU_fd);

			list_add(lista_CPU,(void*)&cpu_new);

		}
		else{
			log_error(log_SAFA,"Error al conectar con un CPU");
		}

	}


	//--------------------------------------------------------------------------------------------------------






}

//Funcion para detectar el algoritmo de planificacion
t_algoritmo detectarAlgoritmo(char*algoritmo){

	t_algoritmo algo;

	if(string_equals_ignore_case(algoritmo,"RR")){
		algo=RR;
	}else if(string_equals_ignore_case(algoritmo,"VRR")){
		algo=VRR;
	}else if(string_equals_ignore_case(algoritmo,"PROPIO")){
		algo=PROPIO;
	}else{
		algo=FIFO;
	}
	return algo;
}

//Funcion que se encargara de recibir mensajes del DAM
void atenderDAM(int*fd){



}

void atenderCPU(int*fd){



}

