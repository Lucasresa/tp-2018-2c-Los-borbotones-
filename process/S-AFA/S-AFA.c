#include "S-AFA.h"


int main(){

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

	//Aca habria que hacer la parte de la conexion con los demas procesos usando sockets. (Usando la shared library de sockets



	//-------------------------------------------------------------------------------------------------------






}

//Creo el dtb y lo agrego a la cola de New
void agregarDTBANew(char*path){


	t_DTB dtb;

	dtb.escriptorio=string_duplicate(path);
	dtb.f_inicializacion=0;
	dtb.pc=0;
	dtb.id=cont_id;
	cont_id++;

	queue_push(cola_new,(void*)&dtb);

}

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


