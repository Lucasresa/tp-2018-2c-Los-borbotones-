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
				if(args==NULL){
				printf("Cantidad de procesos en las colas:\nNew: %d\nReady: %d\nExec: %d\nBlock: %d\nExit: %d\nCPU libres: %d\n",
						list_size(cola_new),list_size(cola_ready),dictionary_size(cola_exec),dictionary_size(cola_block),
						list_size(cola_exit),list_size(CPU_libres));
				}else{
					t_DTB* dtb_status;
					int dtb_id = (int)strtol(args,(char**)NULL,10),i;
					char* estado;
					estado=buscarDTB(&dtb_status,dtb_id);
					if(estado==NULL){
						printf("No se encontro ningun DTB en el sistema con ese ID\n");
					}else{

						printf("DTB %d\nEstado DTB: %s\nProgram Counter: %d\nQuantum sobrante: %d\nScript: %s\nArchivos abiertos: %d\n",
							dtb_status->id, estado, dtb_status->pc, dtb_status->quantum_sobrante, dtb_status->escriptorio,
								dtb_status->cant_archivos);
						if(dtb_status->cant_archivos!=0){
							for(i=0;i<dtb_status->cant_archivos;i++){
								printf("\n\t%s",dtb_status->archivos[i]);
							}
						}
					}
				}
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

char* buscarDTB(t_DTB** dtb, int id){

	char* estado;

	if((*dtb=buscarDTBEnCola(cola_new,id))!=NULL){
		estado="NEW";
		return estado;
	}else if((*dtb=buscarDTBEnCola(cola_ready,id))!=NULL){
		estado="READY";
		return estado;
	}else if((*dtb=buscarDTBEnCola(cola_ready_VRR,id))!=NULL){
		estado="READY VIRTUAL";
		return estado;
	}else if((*dtb=buscarDTBEnCola(cola_exit,id))!=NULL){
		estado="FINALIZADO";
		return estado;
	}else if(dictionary_has_key(cola_exec,string_itoa(id))){
		*dtb=dictionary_get(cola_exec,string_itoa(id));
		estado="EJECUTANDO";
		return estado;
	}else if(dictionary_has_key(cola_block,string_itoa(id))){
		*dtb=dictionary_get(cola_block,string_itoa(id));
		estado="BLOQUEADO";
		return estado;
	}else{
		return NULL;
	}
}

t_DTB* buscarDTBEnCola(t_list* cola, int id){
	int i;

	t_list* cola_copy = list_duplicate(cola);

	t_DTB* dtb=malloc(sizeof(t_DTB));

	for(i=0;i<list_size(cola_copy);i++){

		dtb=list_remove(cola_copy,i);

		if(dtb->id==id){
			list_destroy_and_destroy_elements(cola_copy,free);
			log_info(log_SAFA,"Se encontro el dtb con esa id");
			sleep(1);
			return dtb;
		}
	}
	list_destroy_and_destroy_elements(cola_copy,free);
	return NULL;
}

//----------------------------------------------------------------------------------------------------------
//Funciones planificador.

//Creo el dtb y lo agrego a la cola de New
void agregarDTBDummyANew(char*path,t_DTB*dtb){

	dtb->escriptorio=string_duplicate(path);
	dtb->f_inicializacion=0;
	dtb->pc=0;
	dtb->quantum_sobrante=0; //Cuando este valor sea 0, el CPU sabra que tiene que ejecutar el quantum completo
	dtb->id=cont_id;
	dtb->cant_archivos=0;
	cont_id++;

	list_add(cola_new,dtb);

	log_info(log_SAFA,"Agregado el DTB_dummy %d a la cola de new",dtb->id);

	//Le digo al PLP que se ejecute y que decida si hay que enviar algun proceso a Ready

	ejecutarPLP();

}


//Planificador a largo plazo
void ejecutarPLP(){
	//Si el grado de multiprogramacion lo permite, entonces le aviso al PCP para que desbloquee el dummy

	if(config_SAFA.multiprog==0){
		log_warning(log_SAFA,"El grado de multiprogramacion no permite agregar mas procesos a ready");
	}else if(list_size(cola_new)==0){
		log_warning(log_SAFA,"La cola de new esta vacia");
	}else{
		t_DTB* init_dummy;

		init_dummy=list_remove(cola_new,0);

		config_SAFA.multiprog = config_SAFA.multiprog - 1;

		log_info(log_SAFA,"Multiprogramacion actual= %d",config_SAFA.multiprog);

		log_info(log_SAFA,"El Dtb dummy %d se agrego a la cola de ready",init_dummy->id);

		list_add(cola_ready,init_dummy);

		log_info(log_SAFA,"Se ejecutara el PCP para desbloquear el dummy");

		ejecutarPCP(EJECUTAR_PROCESO,NULL);
	}
	usleep(config_SAFA.retardo*1000);
}

//Planificador a corto plazo
void ejecutarPCP(int operacion, t_DTB* dtb){

	t_DTB* dtb_aux = dtb;

	switch(operacion){
	case EJECUTAR_PROCESO:
		//Si no hay CPUs libres entonces no hace nada
		if(list_size(CPU_libres)==0){
				log_warning(log_SAFA,"Todas las CPU estan ejecutando");
		}
		else{
			switch(config_SAFA.algoritmo){
				case VRR:
					algoritmo_VRR(dtb);
					break;
				default:
					algoritmo_FIFO_RR(dtb);
					break;
				}
			}
		break;
	case BLOQUEAR_PROCESO:
		log_info(log_SAFA,"Bloqueando el DTB %d",dtb->id);
		if(config_SAFA.algoritmo!=VRR){
			dtb->quantum_sobrante=0;
		}
		dictionary_put(cola_block,string_itoa(dtb->id),dtb);
		break;
	case FINALIZAR_PROCESO:
		log_info(log_SAFA,"El DTB %d finalizo su ejecucion",dtb->id);
		list_add(cola_exit,dtb);
		config_SAFA.multiprog+=1;
		break;
	case FIN_QUANTUM:
		log_info(log_SAFA,"El DTB %d se quedo sin quantum",dtb->id);
		list_add(cola_ready,dtb);
		break;
	}

	usleep(config_SAFA.retardo*1000);

}
//Envio a la CPU el DTB para que ejecute
void ejecutarProceso(t_DTB* proceso,int CPU_vacio){

		void*buffer;

		dictionary_put(cola_exec,string_itoa(CPU_vacio),proceso);

		log_info(log_SAFA,"Enviando info del quantum al CPU %d...",CPU_vacio);

		send(CPU_vacio,&config_SAFA.quantum,sizeof(int),0);

		serializarYEnviarDTB(CPU_vacio,buffer,*proceso);

		log_info(log_SAFA,"DTB enviado con exito!");

}
//Algoritmo FIFO y RR
void algoritmo_FIFO_RR(t_DTB* dtb){
	//Verifico que haya procesos en la cola de Ready normal
	if(list_size(cola_ready)==0){
		log_warning(log_SAFA,"Cola de ready vacia, el CPU no puede ejecutar nada");
	}else{
		dtb=list_remove(cola_ready,0);

		int CPU_vacio=(int)list_remove(CPU_libres,0);

		log_info(log_SAFA,"El DTB %d esta listo para ser ejecutado",dtb->id);

		log_info(log_SAFA,"Se envio el DTB a ejecutar en el CPU %d",CPU_vacio);

		//Retardo en la planificacion....
		usleep(config_SAFA.retardo*1000);

		ejecutarProceso(dtb,CPU_vacio);
	}
}
//Algoritmo VRR
void algoritmo_VRR(t_DTB* dtb){

	if(list_size(cola_ready_VRR)==0){
		log_warning(log_SAFA,"La cola virtual esta vacia... ejecuto como si fuera RR/FIFO");
		algoritmo_FIFO_RR(dtb);
	}else{

		dtb=list_remove(cola_ready_VRR,0);

		int CPU_vacio=(int)list_remove(CPU_libres,0);

		log_info(log_SAFA,"El DTB %d esta listo para ser ejecutado",dtb->id);

		log_info(log_SAFA,"Se envio el DTB a ejecutar en el CPU %d",CPU_vacio);

		usleep(config_SAFA.retardo*1000);

		ejecutarProceso(dtb,CPU_vacio);

	}

}
void algoritmo_PROPIO(t_DTB* dtb){

}
