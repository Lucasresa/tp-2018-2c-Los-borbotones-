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
			{
				if(args==NULL){
					printf("ERROR: no escribio ninguna ruta, por favor escriba 'help'\n");
				}else{
					t_DTB* dtb_new=malloc(sizeof(t_DTB));
					agregarDTBDummyANew(args,dtb_new);
				}
				break;
			}
			case 2:
				if(args==NULL){
				pthread_mutex_lock(&File_config);
				int total_procesos=getCantidadProcesosInicializados(cola_block)+dictionary_size(cola_exec)+getCantidadProcesosInicializados(cola_ready)+
								getCantidadProcesosInicializados(cola_ready_IOBF)+getCantidadProcesosInicializados(cola_ready_VRR);
				printf("Cantidad de procesos en las colas:\nNew: %d\nReady: %d\nExec: %d\nBlock: %d\nExit: %d\nCPU libres: %d\n",
						list_size(cola_new),list_size(cola_ready),dictionary_size(cola_exec),list_size(cola_block),
						list_size(cola_exit),list_size(CPU_libres));
					if(config_SAFA.algoritmo==VRR){
						printf("Virtual: %d\n",list_size(cola_ready_VRR));
					}else if(config_SAFA.algoritmo==IOBF){
						printf("IOBF: %d\n",list_size(cola_ready_IOBF));
					}
				printf("Total de procesos Inicializados: %d\n",total_procesos);
				printf("Grado de Multiprogramacion General: %d\n",config_SAFA.multiprog);
				printf("Grado de Multiprogramacion Actual: %d\n",multiprogramacion_actual);
				printf("Retardo de planificacion: %d\n",config_SAFA.retardo);
				printf("Quantum actual: %d\nSi es 0 significa que el sistema usa FIFO\n",config_SAFA.quantum);
				pthread_mutex_unlock(&File_config);
				}else{
					t_DTB* dtb_status;
					int dtb_id = (int)strtol(args,(char**)NULL,10),i;
					char* estado;
					estado=buscarDTB(&dtb_status,dtb_id,STATUS);
					if(estado==NULL){
						printf("No se encontro ningun DTB en el sistema con ese ID\n");
					}else{

						printf("DTB %d\nEstado DTB: %s\nProgram Counter: %d\nQuantum sobrante: %d\nScript: %s\nArchivos abiertos: %d\n",
							dtb_status->id, estado, dtb_status->pc, dtb_status->quantum_sobrante, dtb_status->escriptorio,
								list_size(dtb_status->archivos));
						if(dtb_status->f_inicializacion==0){
							printf("Tipo: DTB Dummy\n");
						}else{
							printf("Tipo: DTB Inicializado\n");
						}
						if(list_size(dtb_status->archivos)!=0){
							t_archivo* archivo;
							for(i=0;i<list_size(dtb_status->archivos);i++){
								archivo=list_get(dtb_status->archivos,i);
								printf("\t%s\n",archivo->path);
							}
						}
					}
				}
				break;
			case 3:
				if(args==NULL){
					printf("ERROR: se debe ingresar un id por favor escriba 'help'\n");
				}else{
					t_DTB* dtb;
					int dtb_id = (int)strtol(args,(char**)NULL,10);
					char* estado;
					estado=buscarDTB(&dtb,dtb_id,FINALIZAR);

					if(estado==NULL){
						printf("No se encontro ningun DTB en el sistema con ese ID\n");
					}else if(string_equals_ignore_case(estado,"EJECUTANDO")){
						printf("No se puede finalizar este proceso, se encuentra ejecutando\n");
					}else{
						pthread_mutex_lock(&mx_PCP);
						ejecutarPCP(FINALIZAR_PROCESO,dtb);
						pthread_mutex_unlock(&mx_PCP);
						//Ejecuto PLP para ver si puedo agregar algun proceso a ready
						pthread_mutex_lock(&mx_PLP);
						ejecutarPLP();
						pthread_mutex_unlock(&mx_PLP);

						//Aca deberia avisar a DAM, en caso de que sea un proceso inicializado, que el proceso finalizo y que le diga a
						//Fm9 que libere la memoria que dicho proceso estaba ocupando


					}
				}
			break;
			case 4:
				pthread_mutex_lock(&mx_metricas);
				if(args==NULL){

					float sentencias_DAM, sentencias_Totales, sentencias_Exit, total_Procesos, procesos_Exit;
					float promedio_sentencias_DAM, promedio_sentencias_Exit;
					float porcentaje_sentencias_DAM;

					sentencias_DAM = (float)getSentenciasDAM();
					sentencias_Totales = (float)getSentenciasTotales();
					sentencias_Exit = (float)getSentenciasParaExit();
					total_Procesos = (float)list_size(info_metricas);
					procesos_Exit = (float)list_size(cola_exit);

					if(procesos_Exit!=0)
						promedio_sentencias_Exit = sentencias_Exit/procesos_Exit;
					else
						promedio_sentencias_Exit = 0;

					if(total_Procesos!=0)
						promedio_sentencias_DAM = sentencias_DAM/total_Procesos;
					else
						promedio_sentencias_DAM = 0;

					if(sentencias_Totales!=0)
						porcentaje_sentencias_DAM = (sentencias_DAM/sentencias_Totales)*100;
					else
						porcentaje_sentencias_DAM = 0;

					printf("Cantidad de sentencias ejecutadas promedio del sistema que usaron a 'El diego': %0.2f\n"
							"Cantidad de sentencias ejecutadas promedio del sistema para que un DTB termine	en la cola EXIT: %0.2f\n"
							"Porcentaje de las sentencias ejecutadas promedio que fueron a 'El Diego': %0.2f%%\n"
							"Tiempo de Respuesta promedio del Sistema: ??\n",
							promedio_sentencias_DAM, promedio_sentencias_Exit,porcentaje_sentencias_DAM);

				}else{
					//Debo mostrar la cantidad de sentencias que espero el DTB en NEW
					t_DTB* dtb_new;
					int dtb_id = (int)strtol(args,(char**)NULL,10);
					char* estado=buscarDTB(&dtb_new,dtb_id,0);
					if(estado==NULL){
						printf("El DTB no existe en el sistema\n");
					}else if(dtb_new->f_inicializacion==0){
						printf("DTB dummy, no tiene metricas\n");
					}
					else{
						t_metricas* metrica_dtb=buscarMetricasDTB(dtb_new->id);
						printf("DTB %d\nCantidad de sentencias ejecutadas: %d\nCantidad de sentencias en NEW: %d\nCantidad de sentencias a DAM: %d\n",
								metrica_dtb->id_dtb,metrica_dtb->sent_ejecutadas,metrica_dtb->sent_NEW,metrica_dtb->sent_DAM);
					}
				}
			pthread_mutex_unlock(&mx_metricas);
			break;
			case 5:
				printf("Operaciones disponibles:\n\tejecutar <path archivo>\n\tstatus <ID-opcional->\n\tfinalizar <ID>\n\tmetricas <ID>\n");
			break;
			default:
				printf("No se reconoce la operacion, escriba 'help' para ver las operaciones disponibles\n");
	}
}

//-----------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------
//Funciones de busqueda de un DTB en las colas por su ID


char* buscarDTB(t_DTB** dtb, int id, int operacion){

	char* estado;

	if((*dtb=buscarDTBEnCola(cola_new,id,operacion))!=NULL){
		estado="NEW";
		return estado;
	}else if((*dtb=buscarDTBEnCola(cola_ready,id,operacion))!=NULL){
		estado="READY";
		return estado;
	}else if((*dtb=buscarDTBEnCola(cola_ready_VRR,id,operacion))!=NULL){
		estado="READY VIRTUAL";
		return estado;
	}else if((*dtb=buscarDTBEnCola(cola_exit,id,operacion))!=NULL){
		estado="FINALIZADO";
		return estado;
	}else if(dictionary_has_key(cola_exec,string_itoa(id))){
		*dtb=dictionary_get(cola_exec,string_itoa(id));
		estado="EJECUTANDO";
		return estado;
	}else if((*dtb=buscarDTBEnCola(cola_block,id,operacion))!=NULL){
		estado="BLOQUEADO";
		return estado;
	}else{
		return NULL;
	}
}

t_DTB* buscarDTBEnCola(t_list* cola, int id, int operacion){
	int i;

	t_list* cola_copy = list_duplicate(cola);

	t_DTB* dtb;

	for(i=0;i<list_size(cola_copy);i++){

		dtb=list_get(cola_copy,i);

		if(dtb->id==id){
			list_clean(cola_copy);
			list_destroy(cola_copy);
			log_info(log_SAFA,"Se encontro el dtb con esa id");
			if(operacion==FINALIZAR){
				pthread_mutex_lock(&mx_colas);
				list_remove(cola,i);
				pthread_mutex_unlock(&mx_colas);
			}
			return dtb;
		}
	}
	list_clean(cola_copy);
	list_destroy(cola_copy);
	return NULL;
}

t_DTB* getDTBEnCola(t_list* cola,int id){

	int i;
	t_DTB* dtb;

	for(i=0;i<list_size(cola);i++){

		dtb=list_get(cola,i);

		if(dtb->id==id){
			dtb=list_remove(cola,i);

			return dtb;
		}
	}
	return NULL;
}

//----------------------------------------------------------------------------------------------------------
//_---------------------------------------------------------------------------------------------------------
//Funciones para las metricas

t_metricas* buscarMetricasDTB(int id){
	int i;
	int dtb_totales=list_size(info_metricas);
	t_metricas* metrica;
	for(i=0;i<dtb_totales;i++){
		metrica=list_get(info_metricas,i);
		if(metrica->id_dtb==id){
			return metrica;
		}
	}

	return NULL;
}

void actualizarMetricaDTB(int id, int protocolo){

	int i, dtb_totales=list_size(info_metricas);

	t_metricas* metrica;

	for(i=0;i<dtb_totales;i++){
		metrica=list_get(info_metricas,i);
		if(metrica->id_dtb==id){
			metrica->sent_ejecutadas++;
			if(protocolo==SENTENCIA_DAM)
				metrica->sent_DAM++;
			break;
		}
	}
}

void actualizarMetricasDTBNew(){

	int i, total_new=list_size(cola_new);
	int id_dtb;

	t_metricas* metrica;

	for(i=0;i<total_new;i++){
		id_dtb=((t_DTB*)list_get(cola_new,i))->id;

		log_info(log_SAFA,"Actualizando metrica de espera en NEW para DTB %d",id_dtb);

		metrica=buscarMetricasDTB(id_dtb);

		metrica->sent_NEW++;

	}

}

int getSentenciasDAM(){

	int sumatoria=0,total_procesos=list_size(info_metricas),i;

	t_metricas* metrica;

	for(i=0;i<total_procesos;i++){
		metrica=list_get(info_metricas,i);

		sumatoria+=metrica->sent_DAM;

	}

	return sumatoria;
}

int getSentenciasTotales(){

	int sumatoria=0,total_procesos=list_size(info_metricas),i;

	t_metricas* metrica;

	for(i=0;i<total_procesos;i++){
		metrica=list_get(info_metricas,i);

		sumatoria+=metrica->sent_ejecutadas;

	}

	return sumatoria;
}

int getSentenciasParaExit(){

	int sumatoria=0,total_procesos=list_size(cola_exit),i;

	t_DTB* dtb_aux;

	for(i=0;i<total_procesos;i++){
		dtb_aux=list_get(cola_exit,i);

		sumatoria+=dtb_aux->pc;

	}

	return sumatoria;
}

int getCantidadProcesosInicializados(t_list* cola){

	int i, contador=0;

	t_DTB* aux;

	for(i=0;i<list_size(cola);i++){

		aux=list_get(cola,i);

		if(aux->f_inicializacion==1){
			contador++;
		}
	}
	return contador;
}


//----------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------
//Funciones planificador.

//Creo el dtb y lo agrego a la cola de New
void agregarDTBDummyANew(char*path,t_DTB*dtb){

	dtb->escriptorio=string_duplicate(path);
	dtb->f_inicializacion=0;
	dtb->pc=0;
	dtb->quantum_sobrante=0; //Cuando este valor sea 0, el CPU sabra que tiene que ejecutar el quantum completo
	dtb->id=cont_id;
	dtb->archivos=list_create();
	cont_id++;

	pthread_mutex_lock(&mx_colas);
	list_add(cola_new,dtb);
	pthread_mutex_unlock(&mx_colas);

	log_info(log_SAFA,"Agregado el DTB_dummy %d a la cola de new",dtb->id);

	//Le digo al PLP que se ejecute y que decida si hay que enviar algun proceso a Ready
	pthread_mutex_lock(&mx_PLP);
	ejecutarPLP();
	pthread_mutex_unlock(&mx_PLP);
}


//Planificador a largo plazo
void ejecutarPLP(){
	//Si el grado de multiprogramacion lo permite, entonces le aviso al PCP para que desbloquee el dummy
	if(multiprogramacion_actual==0){
		log_warning(log_SAFA,"El grado de multiprogramacion no permite agregar mas procesos a ready");

		int total_procesos_memoria = getCantidadProcesosInicializados(cola_block)+dictionary_size(cola_exec)+getCantidadProcesosInicializados(cola_ready)+
				getCantidadProcesosInicializados(cola_ready_IOBF)+getCantidadProcesosInicializados(cola_ready_VRR);

		if(total_procesos_memoria>config_SAFA.multiprog){

			//Tengo que pasar los procesos que esten de mas en Ready a la cola de NEW

			int fin_cola, procesos_a_sacar=total_procesos_memoria-config_SAFA.multiprog;
			int i;

			t_DTB* dtb;

			log_warning(log_SAFA,"Hay mas procesos de lo permitido por el grado de multiprogramacion");
			log_info(log_SAFA,"Se pasaran los procesos que esten de mas a la cola NEW");

			switch(config_SAFA.algoritmo){
			case IOBF:
				fin_cola=list_size(cola_ready)-1;

				for(i=0;i<procesos_a_sacar&&list_size(cola_ready)>0;i++){

					dtb=list_remove(cola_ready,fin_cola);

					list_add(cola_new,dtb);

					fin_cola--;
				}

				total_procesos_memoria = getCantidadProcesosInicializados(cola_block)+dictionary_size(cola_exec)+
						getCantidadProcesosInicializados(cola_ready)+getCantidadProcesosInicializados(cola_ready_IOBF)+
							getCantidadProcesosInicializados(cola_ready_VRR);

				procesos_a_sacar=total_procesos_memoria-config_SAFA.multiprog;
				if(procesos_a_sacar>0){

					fin_cola=list_size(cola_ready_IOBF)-1;

					for(i=0;i<procesos_a_sacar&&list_size(cola_ready_IOBF)>0&&fin_cola>-1;fin_cola--){

						dtb=list_get(cola_ready_IOBF,fin_cola);

						if(dtb->f_inicializacion==1){

							dtb=list_remove(cola_ready_IOBF,fin_cola);

							list_add(cola_new,dtb);

							i++;
						}

					}
				}
				break;
			case VRR:
				fin_cola=list_size(cola_ready)-1;

				for(i=0;i<procesos_a_sacar&&list_size(cola_ready)>0&&fin_cola>-1;fin_cola--){

					dtb=list_get(cola_ready,fin_cola);

					if(dtb->f_inicializacion==1){

						dtb=list_remove(cola_ready,fin_cola);

						list_add(cola_new,dtb);

						i++;

					}
				}

				total_procesos_memoria = getCantidadProcesosInicializados(cola_block)+dictionary_size(cola_exec)+
						getCantidadProcesosInicializados(cola_ready)+getCantidadProcesosInicializados(cola_ready_IOBF)+
							getCantidadProcesosInicializados(cola_ready_VRR);

				procesos_a_sacar=total_procesos_memoria-config_SAFA.multiprog;
				if(procesos_a_sacar>0){

					fin_cola=list_size(cola_ready_VRR)-1;

					for(i=0;i<procesos_a_sacar&&list_size(cola_ready_VRR)>0;i++){

						dtb=list_remove(cola_ready_VRR,fin_cola);

						dtb->quantum_sobrante=0;

						list_add(cola_new,dtb);

						fin_cola--;

					}
				}
				break;
			default:
				fin_cola=list_size(cola_ready)-1;

				for(i=0;i<procesos_a_sacar&&list_size(cola_ready)>0&&fin_cola>-1;fin_cola--){

					dtb=list_get(cola_ready,fin_cola);

					if(dtb->f_inicializacion==1){

						dtb=list_remove(cola_ready,fin_cola);

						list_add(cola_new,dtb);

						i++;
					}

				}
				break;
			}
		}
	}else if(list_size(cola_new)==0){
		log_warning(log_SAFA,"La cola de new esta vacia");
	}else{
		t_DTB* init_dummy;

		for(;list_size(cola_new)!=0&&multiprogramacion_actual!=0;){

			pthread_mutex_lock(&mx_colas);
			init_dummy=list_remove(cola_new,0);

			if(init_dummy->f_inicializacion!=0){
				multiprogramacion_actual = multiprogramacion_actual - 1;
				log_info(log_SAFA,"Multiprogramacion actual= %d",multiprogramacion_actual);
				log_info(log_SAFA,"Se ejecutara el PCP para enviar el proceso %d a ready",init_dummy->id);
			}
			else{
				log_info(log_SAFA,"El Dtb dummy %d se agrego a la cola de ready",init_dummy->id);
				log_info(log_SAFA,"Se ejecutara el PCP para desbloquear el dummy");
			}

			if(config_SAFA.algoritmo==IOBF)
				list_add(cola_ready_IOBF,init_dummy);
			else
				list_add(cola_ready,init_dummy);

			pthread_mutex_unlock(&mx_colas);

			pthread_mutex_lock(&mx_PCP);
			ejecutarPCP(EJECUTAR_PROCESO,NULL);
			pthread_mutex_unlock(&mx_PCP);
		}

	}
	usleep(config_SAFA.retardo*1000);
}

//Planificador a corto plazo
void ejecutarPCP(int operacion, t_DTB* dtb){

	t_DTB* dtb_aux;
	int total_procesos_memoria = getCantidadProcesosInicializados(cola_block)+dictionary_size(cola_exec)+
			getCantidadProcesosInicializados(cola_ready)+getCantidadProcesosInicializados(cola_ready_IOBF)+
				getCantidadProcesosInicializados(cola_ready_VRR);


	pthread_mutex_lock(&File_config);
	pthread_mutex_lock(&mx_metricas);
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
				case IOBF:
					algoritmo_PROPIO(dtb);
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
		pthread_mutex_lock(&mx_colas);
		list_add(cola_block,dtb);
		pthread_mutex_unlock(&mx_colas);
		break;
	case DESBLOQUEAR_DUMMY:
		log_info(log_SAFA,"Desbloqueando el DTB Dummy %d",dtb->id);
		pthread_mutex_lock(&mx_colas);
		dtb=getDTBEnCola(cola_block,dtb->id);
		dtb->f_inicializacion=1;

		//Cargo informacion necesaria del DTB para el manejo de metricas
		t_metricas* metrica_dtb=malloc(sizeof(t_metricas));
		metrica_dtb->id_dtb=dtb->id;
		metrica_dtb->sent_DAM=0;
		metrica_dtb->sent_ejecutadas=0;
		metrica_dtb->sent_NEW=0;
		//Agrego esa informacion a esta lista auxiliar
		list_add(info_metricas,metrica_dtb);

		list_add(cola_new,dtb);
		pthread_mutex_unlock(&mx_colas);
		break;
	case DESBLOQUEAR_PROCESO:
		log_info(log_SAFA,"Desbloqueando el DTB %d",dtb->id);

		pthread_mutex_lock(&mx_colas);
		dtb_aux=getDTBEnCola(cola_block,dtb->id);

		if(config_SAFA.algoritmo==VRR&&dtb_aux->quantum_sobrante>0)
			list_add(cola_ready_VRR,dtb_aux);
		else if(config_SAFA.algoritmo==IOBF)
			list_add(cola_ready_IOBF,dtb_aux);
		else
			list_add(cola_ready,dtb_aux);

		if(total_procesos_memoria>config_SAFA.multiprog){
			pthread_mutex_lock(&mx_PLP);
			ejecutarPLP();
			pthread_mutex_unlock(&mx_PLP);
		}

		pthread_mutex_unlock(&mx_colas);
		break;
	case FINALIZAR_PROCESO:
		log_info(log_SAFA,"El DTB %d finalizo su ejecucion",dtb->id);
		pthread_mutex_lock(&mx_colas);
		list_add(cola_exit,dtb);
		pthread_mutex_unlock(&mx_colas);

		//Debo avisar a DAM que finalizo el DTB (en caso de no ser dummy) para que avise a fm9 que debe liberar el espacio

		if(total_procesos_memoria<config_SAFA.multiprog){
			if(dtb->f_inicializacion!=0)
				multiprogramacion_actual+=1;
		}
		break;
	case FIN_QUANTUM:
		log_info(log_SAFA,"El DTB %d se quedo sin quantum",dtb->id);

		pthread_mutex_lock(&mx_colas);
		list_add(cola_ready,dtb);
		pthread_mutex_unlock(&mx_colas);

		if(total_procesos_memoria>config_SAFA.multiprog){
			pthread_mutex_lock(&mx_PLP);
			ejecutarPLP();
			pthread_mutex_unlock(&mx_PLP);
		}

		break;
	}

	pthread_mutex_unlock(&mx_metricas);
	pthread_mutex_unlock(&File_config);
	usleep(config_SAFA.retardo*1000);

}
//Envio a la CPU el DTB para que ejecute
void ejecutarProceso(t_DTB* proceso,int CPU_vacio){

		void*buffer;

		int id_dtb = proceso->id;

		pthread_mutex_lock(&mx_colas);
		dictionary_put(cola_exec,string_itoa(id_dtb),proceso);
		pthread_mutex_unlock(&mx_colas);

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
		pthread_mutex_lock(&mx_colas);
		dtb=list_remove(cola_ready,0);
		pthread_mutex_unlock(&mx_colas);

		pthread_mutex_lock(&mx_CPUs);
		int CPU_vacio=(int)list_remove(CPU_libres,0);
		pthread_mutex_unlock(&mx_CPUs);

		log_info(log_SAFA,"El DTB %d esta listo para ser ejecutado",dtb->id);

		log_info(log_SAFA,"Se envio el DTB a ejecutar en el CPU %d",CPU_vacio);

		ejecutarProceso(dtb,CPU_vacio);
	}
}
//Algoritmo VRR
void algoritmo_VRR(t_DTB* dtb){

	if(list_size(cola_ready_VRR)==0){
		log_warning(log_SAFA,"La cola virtual esta vacia... ejecuto como si fuera RR/FIFO");
		algoritmo_FIFO_RR(dtb);
	}else{

		pthread_mutex_lock(&mx_colas);
		dtb=list_remove(cola_ready_VRR,0);
		pthread_mutex_unlock(&mx_colas);

		pthread_mutex_lock(&mx_CPUs);
		int CPU_vacio=(int)list_remove(CPU_libres,0);
		pthread_mutex_unlock(&mx_CPUs);

		log_info(log_SAFA,"El DTB %d esta listo para ser ejecutado",dtb->id);

		log_info(log_SAFA,"Se envio el DTB a ejecutar en el CPU %d",CPU_vacio);

		ejecutarProceso(dtb,CPU_vacio);

	}

}
void algoritmo_PROPIO(t_DTB* dtb){

	if(list_size(cola_ready_IOBF)==0){
		log_warning(log_SAFA,"La cola de mayor prioridad esta vacia... ejecuto procesos en la de menor prioridad");
		algoritmo_FIFO_RR(dtb);
	}else{

		pthread_mutex_lock(&mx_colas);
		dtb=list_remove(cola_ready_IOBF,0);
		pthread_mutex_unlock(&mx_colas);

		pthread_mutex_lock(&mx_CPUs);
		int CPU_vacio=(int)list_remove(CPU_libres,0);
		pthread_mutex_unlock(&mx_CPUs);

		log_info(log_SAFA,"El DTB %d esta listo para ser ejecutado",dtb->id);

		log_info(log_SAFA,"Se envio el DTB a ejecutar en el CPU %d",CPU_vacio);

		ejecutarProceso(dtb,CPU_vacio);

	}
}
