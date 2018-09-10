#include "S-AFA.h"


int main(){

	//Creo las colas del S-AFA

	cola_new = queue_create();
	cola_ready = queue_create();
	cola_block = queue_create();
	cola_exec = queue_create();
	cola_exit = queue_create();

	//Levanto archivo de configuracion del S-AFA

	file=config_create("CONFIG_S-AFA.cfg");

	config.algoritmo=malloc(sizeof(char)*6);

	config.port=config_get_int_value(file,"PUERTO");
	config.algoritmo=config_get_string_value(file,"ALGORITMO");
	config.quantum=config_get_int_value(file,"QUANTUM");
	config.multiprog=config_get_int_value(file,"MULTIPROGRAMACION");
	config.retardo=config_get_int_value(file,"RETARDO_PLANIF");

	config_destroy(file);

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

	queue_push(cola_new,(void*)dtb);

}


