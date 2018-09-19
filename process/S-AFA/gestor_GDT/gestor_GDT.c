#include "gestor_GDT.h"
/*
int main(){
	consola();
	return 0;
}
*/

int cont_id=0;


void* consola(void){
	char * linea;
	char **linea_parseada;
	int nro_proceso;
	int status=1;
	do{
		linea = readline("$ ");
		if(strlen(linea)!=0)
			add_history(linea);
		linea_parseada = parsearLinea(linea);
		if(*linea_parseada!=NULL){
			nro_proceso= identificarProceso(linea_parseada);
			ejecutarComando(nro_proceso,linea_parseada[1]);
			string_iterate_lines(linea_parseada,(void*)free);
			free(linea_parseada);
		}

		free(linea);
	}while(status);
}

char** parsearLinea(char* linea){
	char * espacio = " ";
	char** linea_parseada = string_split(linea,espacio);
	return linea_parseada;
}
int identificarProceso(char** linea_parseada){

	int i = 0,nro_op = 0,cantidad_operaciones = 5;

	char* operacion[cantidad_operaciones];

	operacion[0]="ejecutar";
	operacion[1]="status";
	operacion[2]="finalizar";
	operacion[3]="metricas";
	operacion[4]="help";

	for(i=0;i<cantidad_operaciones;i++){
		if(strcmp(linea_parseada[0],operacion[i])==0){
			nro_op=i+1;
			return nro_op;
		}
	}

	return -1;
}

void ejecutarComando(int nro_op, char * args){
	switch(nro_op){
			case 1:
				printf("La operacion es ejecutar y el parametro es: %s \n",args);
				agregarDTBDummyANew(args);
			break;
			case 2:
				printf("La operacion es status y el parametro es: %s \n",args);
			break;
			case 3:
				printf("La operacion es finalizar y el parametro es: %s \n",args);
				log_destroy(log_SAFA);
				exit(4);
			break;
			case 4:
				printf("La operacion es metricas y el parametro es: %s \n",args);
			break;
			case 5:
				printf("Operaciones disponibles:\n\tejecutar <path archivo>\n\tstatus <ID-opcional->\n\tfinalizar <ID>\n\tmetricas <ID>\n");
			break;
			default:
				printf("No se reconoce la operacion, escriba 'help' para ver las operaciones disponibles\n");
	}
}

//----------------------------------------------------------------------------------------
//Funciones planificador.

//Creo el dtb y lo agrego a la cola de New
void agregarDTBDummyANew(char*path){

	t_DTB dtb;

	dtb.escriptorio=string_duplicate(path);
	dtb.f_inicializacion=1;
	dtb.pc=0;
	dtb.id=cont_id;
	cont_id++;

	queue_push(cola_new,(void*)&dtb);

	log_info(log_SAFA,"Agregado el DTB_dummy %d a la cola de new",dtb.id);

	//Le digo al PLP que se ejecute y que decida si hay que enviar algun proceso a Ready

	ejecutarPLP();

}


//Planificador a largo plazo
void ejecutarPLP(){

	//Si el grado de multiprogramacion lo permite, entonces le aviso al PCP para que desbloquee el dummy
	if(config_SAFA.multiprog!=0){

		t_DTB init_dummy;

		config_SAFA.multiprog-=1;

		init_dummy=*(t_DTB*)queue_pop(cola_new);

		log_info(log_SAFA,"El Dtb dummy %d se agrego a la cola de ready",init_dummy.id);

		init_dummy.f_inicializacion=0;

		queue_push(cola_ready,(void*)&init_dummy);

		log_info(log_SAFA,"Se ejecutara el PCP para desbloquear el dummy");

		ejecutarPCP();
	}

}

//Planificador a corto plazo
void ejecutarPCP(){
	t_DTB dtb_exec;
	switch(config_SAFA.algoritmo){
	case FIFO:

		if(queue_is_empty(cola_exec)){

			dtb_exec=*(t_DTB*)queue_pop(cola_ready);

			queue_push(cola_exec,(void*)&dtb_exec);

			log_info(log_SAFA,"El DTB %d esta listo para ser ejecutado",dtb_exec.id);

			ejecutarProceso();

		}

	}

}
//Envio a la CPU el DTB para que ejecute
void ejecutarProceso(){

	t_estado_CPU* CPU_vacio;

	t_list* CPU_vacios=list_filter(lista_CPU,estaLibre);

	CPU_vacio=list_get(CPU_vacios,0);

	if(CPU_vacio==NULL){
		log_warning(log_SAFA,"No hay ningun CPU vacio, el proceso volvera a Ready");
		queue_push(cola_ready,queue_pop(cola_exec));
	}
	else{
		//Aca deberia enviarse el DTB al CPU que se encuentra disponible.
		log_info(log_SAFA,"Se envio el DTB a ejecutar en el CPU %d",CPU_vacio->CPU_fd);

	}

}

bool estaLibre(void* estado){
	t_estado_CPU* estado_CPU=(t_estado_CPU*)estado;
	return !estado_CPU->estado;
}

