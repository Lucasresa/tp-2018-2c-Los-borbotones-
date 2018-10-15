#include "gestor_GDT.h"

int cont_id=0;

//Consola del SAFA
void* consola_SAFA(void){
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
				t_DTB* dtb_new=malloc(sizeof(t_DTB));
				agregarDTBDummyANew(args,dtb_new);
			break;
			case 2:
				printf("La operacion es status y el parametro es: %s \n",args);

				printf("Cantidad de procesos en las colas:\nNew: %d\nReady: %d\nExec: %d\nBlock: %d\nExit: %d\nCPU libres: %d\n",
						queue_size(cola_new),queue_size(cola_ready),dictionary_size(cola_exec),dictionary_size(cola_block),
						queue_size(cola_exit),list_size(CPU_libres));
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

//----------------------------------------------------------------------------------------------------------
//Funciones planificador.

//Creo el dtb y lo agrego a la cola de New
void agregarDTBDummyANew(char*path,t_DTB*dtb){

	dtb->escriptorio=string_duplicate(path);
	dtb->f_inicializacion=0;
	dtb->pc=0;
	dtb->id=cont_id;
	dtb->cant_archivos=0;
	cont_id++;

	queue_push(cola_new,dtb);

	log_info(log_SAFA,"Agregado el DTB_dummy %d a la cola de new",dtb->id);

	//Le digo al PLP que se ejecute y que decida si hay que enviar algun proceso a Ready

	ejecutarPLP();

}


//Planificador a largo plazo
void ejecutarPLP(){

	//Si el grado de multiprogramacion lo permite, entonces le aviso al PCP para que desbloquee el dummy
	if(config_SAFA.multiprog==0){
		log_warning(log_SAFA,"El grado de multiprogramacion no permite agregar mas procesos a ready");
	}else if(queue_size(cola_new)==0){
		log_warning(log_SAFA,"La cola de new esta vacia");
	}else{
		t_DTB* init_dummy;

		init_dummy=queue_pop(cola_new);

		config_SAFA.multiprog--;

		log_info(log_SAFA,"Multiprogramacion actual= %d",config_SAFA.multiprog);

		log_info(log_SAFA,"El Dtb dummy %d se agrego a la cola de ready",init_dummy->id);

		queue_push(cola_ready,init_dummy);

		log_info(log_SAFA,"Se ejecutara el PCP para desbloquear el dummy");

		ejecutarPCP();
	}
}

//Planificador a corto plazo
void ejecutarPCP(){

	t_DTB* dtb;

	//Si no hay CPUs libres entonces no hace nada
	if(list_size(CPU_libres)==0){
		log_warning(log_SAFA,"Todas las CPU estan ejecutando");
	}
	else{
		//Si no hay procesos para ejecutar en Ready tampoco hace nada
		if(queue_size(cola_ready)==0){
			log_warning(log_SAFA,"Cola de ready vacia, el CPU no puede ejecutar nada");
		}
		else{
			switch(config_SAFA.algoritmo){
			case FIFO:
				algoritmo_FIFO(dtb);
				break;
			case RR:
				algoritmo_RR(dtb);
				break;
			case VRR:
				algoritmo_VRR(dtb);
				break;
			}
		}
	}

}
//Envio a la CPU el DTB para que ejecute
void ejecutarProceso(t_DTB* proceso,int CPU_vacio){

		void*buffer;

		dictionary_put(cola_exec,string_itoa(CPU_vacio),proceso);

		serializarYEnviarDTB(CPU_vacio,buffer,*proceso);

		log_info(log_SAFA,"DTB enviado con exito!");

}

void algoritmo_FIFO(t_DTB* dtb){

		dtb=queue_pop(cola_ready);

		int CPU_vacio=(int)list_remove(CPU_libres,0);

		log_info(log_SAFA,"El DTB %d esta listo para ser ejecutado",dtb->id);

		log_info(log_SAFA,"Se envio el DTB a ejecutar en el CPU %d",CPU_vacio);

		//Retardo en la planificacion....
		usleep(config_SAFA.retardo*1000);

		ejecutarProceso(dtb,CPU_vacio);
}

void algoritmo_RR(t_DTB* dtb){

}
void algoritmo_VRR(t_DTB* dtb){

}
