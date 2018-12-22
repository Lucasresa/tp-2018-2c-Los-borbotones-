#include "S-AFA.h"

int main(){

	//flag para saber si la consola esta operativa

	int flag_consola=0;

	log_SAFA=log_create("log_SAFA.log","SAFA",false,LOG_LEVEL_INFO);

	CPU_libres=list_create();

	claves=dictionary_create();
	show_claves=list_create();

	//Creo las colas del S-AFA

	crear_colas();

	info_metricas=list_create();

	//Creo lista de semaforos por proceso

	semaforos_dtb=dictionary_create();

	//Levanto archivo de configuracion del S-AFA e inicializo los semaforos

	iniciar_semaforos();

	load_config();

	multiprogramacion_actual=config_SAFA.multiprog;

	//-------------------------------------------------------------------------------------------------------

	//Aca habria que hacer la parte de la conexion con los demas procesos usando sockets. (Usando la shared library de sockets

	int SAFA_fd,DAM_fd;
	crearSocket(&SAFA_fd);

	setearParaEscuchar(&SAFA_fd,config_SAFA.port);

	log_info(log_SAFA,"Escuchando nuevas conexiones....");

	log_info(log_SAFA,"Esperando que se conecte el DAM");

	DAM_fd=aceptarConexion(SAFA_fd);

	if(DAM_fd==-1){
		perror("Error de conexion con DAM");
		log_destroy(log_SAFA);
		exit(1);
	}

	DAM=DAM_fd;

	log_info(log_SAFA,"Conexion exitosa con DAM");

	pthread_t hiloDAM;

	pthread_create(&hiloDAM,NULL,(void*)atenderDAM,(void*)&DAM_fd);

	pthread_detach(hiloDAM);

	//El hilo main se queda esperando que se conecten nuevas CPU

	log_info(log_SAFA,"Esperando que se conecte algun CPU para que comience a funcionar SAFA");

	while(1){

		pthread_t hiloCPU;
		int CPU_fd;

		if((CPU_fd=aceptarConexion(SAFA_fd))!=-1){


			pthread_create(&hiloCPU,NULL,(void*)atenderCPU,(void*)&CPU_fd);
			pthread_detach(hiloCPU);
			log_info(log_SAFA,"Conexion exitosa con la CPU %d",CPU_fd);

			pthread_mutex_lock(&mx_CPUs);
			list_add(CPU_libres,(void*)CPU_fd);
			pthread_mutex_unlock(&mx_CPUs);


		}
		else{
			log_error(log_SAFA,"Error al conectar con un CPU");
		}

	//-------------------------------------------------------------------------------------------------------

	//Tiro un hilo para ejecutar la consola y el inotify

		if(flag_consola==0){
			flag_consola=1;

			pthread_t hiloConsola;

			log_info(log_SAFA,"Consola en linea...");

			pthread_create(&hiloConsola,NULL,(void*)consola_SAFA,NULL);

			pthread_detach(hiloConsola);

			//Hilo para actualizar achivo de configuracion del S-AFA

			pthread_t hilo_inotify;

			pthread_create(&hilo_inotify,NULL,(void*)actualizar_file_config,NULL);

			pthread_detach(hilo_inotify);

		}
	}
}


void eliminarSocketCPU(int fd){

	int tamanio_array=list_size(CPU_libres);
	int array_CPU[tamanio_array];
	int i;
	//Paso la cola a un array
	for(i=0;i<tamanio_array;i++){
		array_CPU[i]=(int)list_remove(CPU_libres,0);
	}
	for(i=0;i<tamanio_array;i++){

		if(array_CPU[i]!=fd){
			list_add(CPU_libres,(void*)array_CPU[i]);
		}
		else
		{
			close(array_CPU[i]);
		}
	}
}

//Funcion que se encargara de recibir mensajes del DAM
void atenderDAM(int*fd){
	int fd_DAM = *fd;
	int* protocolo, id_dtb;
	t_DTB* dtb;
	t_DTB* aux;
	pthread_mutex_t* sem_dtb;

	info_archivo* archivo_nuevo;

	while(1){

		dtb=malloc(sizeof(t_DTB));
		dtb->archivos=list_create();

		protocolo=recibirYDeserializarEntero(fd_DAM);

		if(protocolo==NULL){
			log_error(log_SAFA,"Se perdio la conexion con DAM, cierro SAFA");
			exit(0);
		}

		if(*protocolo!=FINAL_ABRIR){
		id_dtb=*recibirYDeserializarEntero(fd_DAM);

		}else{
		archivo_nuevo=recibirYDeserializar(fd_DAM,*protocolo);
		id_dtb=archivo_nuevo->pid;
		}

		pthread_mutex_lock(&mx_semaforos);
		sem_dtb = dictionary_get(semaforos_dtb,string_itoa(id_dtb));
		pthread_mutex_unlock(&mx_semaforos);
		pthread_mutex_lock(sem_dtb);

		switch(*protocolo){
		case FINAL_CARGA_DUMMY:

			dtb->id=id_dtb;

			pthread_mutex_lock(&mx_PCP);
			ejecutarPCP(DESBLOQUEAR_DUMMY,dtb);
			pthread_mutex_unlock(&mx_PCP);
			pthread_mutex_lock(&mx_PLP);
			ejecutarPLP();
			pthread_mutex_unlock(&mx_PLP);

			break;
		case FINAL_ABRIR:
		{

			log_info(log_SAFA,"Finalizo la apertura del archivo %s para el dtb %d",archivo_nuevo->path,id_dtb);
			t_archivo* archivo=malloc(sizeof(t_archivo));

			pthread_mutex_lock(&mx_colas);
			aux=buscarDTBEnCola(cola_block,id_dtb,STATUS);
			pthread_mutex_unlock(&mx_colas);

			archivo->path=string_duplicate(archivo_nuevo->path);
			archivo->acceso=archivo_nuevo->acceso;
			list_add(aux->archivos,archivo);

			pthread_mutex_lock(&mx_PCP);
			ejecutarPCP(DESBLOQUEAR_PROCESO,aux);
			pthread_mutex_unlock(&mx_PCP);

			pthread_mutex_lock(&mx_PCP);
			ejecutarPCP(EJECUTAR_PROCESO,NULL);
			pthread_mutex_unlock(&mx_PCP);

			free(archivo_nuevo->path);
			free(archivo_nuevo);
			break;
		}
		case FINAL_CREAR:
			dtb->id=id_dtb;

			log_info(log_SAFA,"Creacion de archivo finalizada para el dtb: %d",id_dtb);

			pthread_mutex_lock(&mx_PCP);
			ejecutarPCP(DESBLOQUEAR_PROCESO,dtb);
			pthread_mutex_unlock(&mx_PCP);

			pthread_mutex_lock(&mx_PCP);
			ejecutarPCP(EJECUTAR_PROCESO,NULL);
			pthread_mutex_unlock(&mx_PCP);

			break;
		case FINAL_BORRAR:
			dtb->id=id_dtb;

			log_info(log_SAFA,"Se Borro correctamente el archivo pedido por el dtb %d",id_dtb);

			pthread_mutex_lock(&mx_PCP);
			ejecutarPCP(DESBLOQUEAR_PROCESO,dtb);
			pthread_mutex_unlock(&mx_PCP);

			pthread_mutex_lock(&mx_PCP);
			ejecutarPCP(EJECUTAR_PROCESO,NULL);
			pthread_mutex_unlock(&mx_PCP);

			break;
		case FINAL_FLUSH:
			dtb->id=id_dtb;

			log_info(log_SAFA,"Flush finalizado correctamente");

			pthread_mutex_lock(&mx_PCP);
			ejecutarPCP(DESBLOQUEAR_PROCESO,dtb);
			pthread_mutex_unlock(&mx_PCP);

			pthread_mutex_lock(&mx_PCP);
			ejecutarPCP(EJECUTAR_PROCESO,NULL);
			pthread_mutex_unlock(&mx_PCP);

			break;
		case ERROR_ARCHIVO_EXISTENTE:
			log_error(log_SAFA,"El dtb %d intento crear un archivo ya existente",id_dtb);
			pthread_mutex_lock(&mx_colas);
			aux=getDTBEnCola(cola_block,id_dtb);
			pthread_mutex_unlock(&mx_colas);

			pthread_mutex_lock(&mx_PCP);
			ejecutarPCP(FINALIZAR_PROCESO,aux);
			pthread_mutex_unlock(&mx_PCP);

			if(aux->f_inicializacion==1){
				pthread_mutex_lock(&mx_PLP);
				ejecutarPLP();
				pthread_mutex_unlock(&mx_PLP);
			}
			break;
		case ERROR_ARCHIVO_INEXISTENTE:
			log_error(log_SAFA,"El dtb %d intento operar sobre un archivo que no existe",id_dtb);
			pthread_mutex_lock(&mx_colas);
			aux=getDTBEnCola(cola_block,id_dtb);
			pthread_mutex_unlock(&mx_colas);

			pthread_mutex_lock(&mx_PCP);
			ejecutarPCP(FINALIZAR_PROCESO,aux);
			pthread_mutex_unlock(&mx_PCP);

			if(aux->f_inicializacion==1){
				pthread_mutex_lock(&mx_PLP);
				ejecutarPLP();
				pthread_mutex_unlock(&mx_PLP);
			}
			break;
		case ERROR_FM9_SIN_ESPACIO:
			log_error(log_SAFA,"No hay suficiente espacio en FM9 para cargar el archivo pedido por el DTB %d",id_dtb);
			pthread_mutex_lock(&mx_colas);
			aux=getDTBEnCola(cola_block,id_dtb);
			pthread_mutex_unlock(&mx_colas);

			pthread_mutex_lock(&mx_PCP);
			ejecutarPCP(FINALIZAR_PROCESO,aux);
			pthread_mutex_unlock(&mx_PCP);

			if(aux->f_inicializacion==1){
				pthread_mutex_lock(&mx_PLP);
				ejecutarPLP();
				pthread_mutex_unlock(&mx_PLP);
			}
			break;
		case ERROR_MDJ_SIN_ESPACIO:
			log_error(log_SAFA,"No hay suficiente espacio en MDJ para cargar lo que hay en FM9",id_dtb);
			pthread_mutex_lock(&mx_colas);
			aux=getDTBEnCola(cola_block,id_dtb);
			pthread_mutex_unlock(&mx_colas);

			pthread_mutex_lock(&mx_PCP);
			ejecutarPCP(FINALIZAR_PROCESO,aux);
			pthread_mutex_unlock(&mx_PCP);

			if(aux->f_inicializacion==1){
				pthread_mutex_lock(&mx_PLP);
				ejecutarPLP();
				pthread_mutex_unlock(&mx_PLP);
			}
			break;

		case ERROR_SEG_MEM:
			log_error(log_SAFA,"Segmentation Fault en memoria provocado por el DTB %d",id_dtb);
			pthread_mutex_lock(&mx_colas);
			aux=getDTBEnCola(cola_block,id_dtb);
			pthread_mutex_unlock(&mx_colas);

			pthread_mutex_lock(&mx_PCP);
			ejecutarPCP(FINALIZAR_PROCESO,aux);
			pthread_mutex_unlock(&mx_PCP);

			if(aux->f_inicializacion==1){
				pthread_mutex_lock(&mx_PLP);
				ejecutarPLP();
				pthread_mutex_unlock(&mx_PLP);
			}
			break;
		}
		list_clean(dtb->archivos);
		list_destroy(dtb->archivos);
		free(dtb);

	}

}

void atenderCPU(int*fd){

	int fd_CPU = *fd;

	int protocolo;

	pthread_mutex_lock(&mx_PCP);
	ejecutarPCP(EJECUTAR_PROCESO,NULL);
	pthread_mutex_unlock(&mx_PCP);

	int dtb_cpu;

	while(1){
		if(recv(fd_CPU,&protocolo,sizeof(int),0)<=0){

			log_error(log_SAFA,"Se desconecto el CPU %d",fd_CPU);
			pthread_mutex_lock(&lock_CPUs);
			//Si el CPU estaba ejecutando un proceso, este se envia a exit y se ejecutara el PLP para que replanifique
			if(dictionary_has_key(cola_exec,string_itoa(dtb_cpu))){
				pthread_mutex_lock(&mx_colas);
				t_DTB* dtb=dictionary_remove(cola_exec,string_itoa(dtb_cpu));
				pthread_mutex_unlock(&mx_colas);

				log_info(log_SAFA,"El CPU %d estaba ejecutando el proceso %d finalizando....",fd_CPU,dtb->id);

				pthread_mutex_lock(&mx_PCP);
				ejecutarPCP(FINALIZAR_PROCESO,dtb);
				pthread_mutex_unlock(&mx_PCP);

				pthread_mutex_lock(&mx_PLP);
				ejecutarPLP();
				pthread_mutex_unlock(&mx_PLP);
			}
			else{ //En caso de no estar ejecutando nada, lo elimino de la lista de CPU_libres
				pthread_mutex_lock(&mx_CPUs);
				eliminarSocketCPU(fd_CPU);
				pthread_mutex_lock(&mx_CPUs);
			}
			pthread_mutex_unlock(&lock_CPUs);
			pthread_exit(NULL);
		}
	//Me fijo que peticion esta haciendo la CPU dependiendo del protocolo que envio
		t_DTB* dtb;
		t_DTB dtb_modificado;

		if(protocolo!=ID_DTB&&protocolo!=SENTENCIA_DAM){
			dtb_modificado=RecibirYDeserializarDTB(fd_CPU);
			pthread_mutex_lock(&mx_colas);
			dtb=dictionary_get(cola_exec,string_itoa(dtb_cpu));
			dtb->pc=dtb_modificado.pc;
			dtb->quantum_sobrante=dtb_modificado.quantum_sobrante;
			pthread_mutex_unlock(&mx_colas);
		}
		char* clave;
		int respuesta;
		//Si recibe un wait de algun recurso
		switch(protocolo){
		case WAIT_RECURSO:
			clave=recibirYDeserializarString(fd_CPU);
			log_info(log_SAFA,"El dtb %d quiere bloquear la clave %s",dtb_cpu,clave);
			pthread_mutex_lock(&mx_claves);
			if(dictionary_has_key(claves,clave)){ //Si existe la clave en el sistema

				t_semaforo* sem_clave=dictionary_remove(claves,clave);

				respuesta=wait_sem(sem_clave,dtb_modificado.id,clave);
				pthread_mutex_unlock(&mx_claves);

				if(respuesta==-1){
					log_error(log_SAFA,"Clave no disponible, otro dtb la esta bloqueando");
				}else{
					log_info(log_SAFA,"Clave bloqueada con exito");
				}

			}else{ //Si no existe la clave en el sistema, tengo que generarla
				log_warning(log_SAFA,"La clave no existe en el sistema, hay que crearla");

				t_semaforo* clave_nueva=malloc(sizeof(t_semaforo));
				clave_nueva->valor=1;
				clave_nueva->cola_bloqueados=list_create();

				char* clave_list = string_duplicate(clave);
				list_add(show_claves,clave_list);
				pthread_mutex_unlock(&mx_claves);

				respuesta=wait_sem(clave_nueva,dtb_modificado.id,clave);
			}
			serializarYEnviarEntero(fd_CPU,&respuesta);
			break;
		//Si recibe un signal de algun recurso
		case SIGNAL_RECURSO:
			clave=recibirYDeserializarString(fd_CPU);

			log_info(log_SAFA,"El dtb %d quiere liberar la clave %s",dtb_cpu,clave);

			pthread_mutex_lock(&mx_claves);
			if(dictionary_has_key(claves,clave)){ //Si existe la clave en el sistema
				t_semaforo* sem_clave=dictionary_remove(claves,clave);

				respuesta=signal_sem(sem_clave,clave);
				pthread_mutex_unlock(&mx_claves);

				log_info(log_SAFA,"La clave existe, continuo con el SIGNAL");

				if(respuesta!=-1){
					dtb=malloc(sizeof(t_DTB));
					dtb->id=respuesta;
					pthread_mutex_lock(&mx_PCP);
					ejecutarPCP(DESBLOQUEAR_PROCESO,dtb);
					pthread_mutex_unlock(&mx_PCP);
					free(dtb);
				}
			}else{ //Si no existe la clave en el sistema, tengo que generarla
				log_warning(log_SAFA,"La clave no existe en el sistema, hay que crearla");
				t_semaforo* clave_nueva = malloc(sizeof(t_semaforo));
				clave_nueva->valor=1;
				clave_nueva->cola_bloqueados=list_create();
				char* clave_list = string_duplicate(clave);
				dictionary_put(claves,clave,clave_nueva);
				list_add(show_claves,clave_list);
				pthread_mutex_unlock(&mx_claves);
			}
			respuesta=SIGNAL_EXITOSO;
			serializarYEnviarEntero(fd_CPU,&respuesta);
			break;
		case ID_DTB:
			dtb_cpu=*recibirYDeserializarEntero(fd_CPU);
			break;
		case SENTENCIA_EJECUTADA:

			log_info(log_SAFA,"El DTB %d ejecuto una sentencia",dtb_cpu);

			pthread_mutex_lock(&lock_CPUs);
			pthread_mutex_lock(&mx_metricas);
			pthread_mutex_lock(&mx_colas);
			actualizarMetricaDTB(dtb_cpu,protocolo);

			actualizarTiempoDeRespuestaDTB();

			actualizarMetricasDTBNew();
			pthread_mutex_unlock(&mx_colas);
			pthread_mutex_unlock(&mx_metricas);
			pthread_mutex_unlock(&lock_CPUs);
			break;
		case SENTENCIA_DAM:

			log_info(log_SAFA,"El DTB %d ejecuto una Entrada/Salida",dtb_cpu);

			pthread_mutex_lock(&lock_CPUs);
			pthread_mutex_lock(&mx_metricas);
			pthread_mutex_lock(&mx_colas);
			actualizarMetricaDTB(dtb_cpu,protocolo);

			actualizarTiempoDeRespuestaDTB();

			actualizarMetricasDTBNew();
			pthread_mutex_unlock(&mx_colas);
			pthread_mutex_unlock(&mx_metricas);
			pthread_mutex_unlock(&lock_CPUs);
			break;
		case FINAL_CERRAR:

			log_info(log_SAFA,"El dtb %d acaba de cerrar uno de sus archivos",dtb_cpu);

			list_clean(dtb->archivos);
			list_add_all(dtb->archivos,dtb_modificado.archivos);

			break;
		case ERROR_SEG_MEM:

			log_error(log_SAFA,"El dtb intento acceder a una posicion invalida de memoria");

			pthread_mutex_lock(&mx_colas);
			dtb=dictionary_remove(cola_exec,string_itoa(dtb_modificado.id));
			pthread_mutex_unlock(&mx_colas);

			pthread_mutex_lock(&mx_PCP);
			ejecutarPCP(FINALIZAR_PROCESO,dtb);
			pthread_mutex_unlock(&mx_PCP);

			break;
		case ERROR_ARCHIVO_INEXISTENTE:

			log_error(log_SAFA,"El dtb intento operar sobre un archivo que no estaba abierto");
			pthread_mutex_lock(&mx_colas);
			dtb=dictionary_remove(cola_exec,string_itoa(dtb_modificado.id));
			pthread_mutex_unlock(&mx_colas);

			pthread_mutex_lock(&mx_PCP);
			ejecutarPCP(FINALIZAR_PROCESO,dtb);
			pthread_mutex_unlock(&mx_PCP);

			break;
		default:

			pthread_mutex_lock(&lock_CPUs);
			pthread_mutex_lock(&mx_colas);
			dtb=dictionary_remove(cola_exec,string_itoa(dtb_modificado.id));
			pthread_mutex_unlock(&mx_colas);

			//El CPU queda libre para ejecutar otro proceso
			pthread_mutex_lock(&mx_CPUs);
			list_add(CPU_libres,(void*)fd_CPU);
			pthread_mutex_unlock(&mx_CPUs);

			//Ejecuto el planificador para que decida que hacer con el dtb dependiendo del protocolo recibido
			pthread_mutex_lock(&mx_PCP);
			ejecutarPCP(protocolo,dtb);
			pthread_mutex_unlock(&mx_PCP);

			if(protocolo==BLOQUEAR_PROCESO){
				pthread_mutex_lock(&mx_semaforos);
				pthread_mutex_t* sem_dtb = dictionary_get(semaforos_dtb,string_itoa(dtb_cpu));
				pthread_mutex_unlock(sem_dtb);

				pthread_mutex_unlock(&mx_semaforos);

			}

			if(protocolo==FINALIZAR_PROCESO){
				log_info(log_SAFA,"El dtb termino de ejecutar su script");
				if(list_size(cola_new)!=0){
					pthread_mutex_lock(&mx_PLP);
					ejecutarPLP();
					pthread_mutex_unlock(&mx_PLP);
				}
			}

			//Vuelvo a ejecutar el planificador pero esta vez para que, de ser posible, asigne otro proceso al CPU
			if(list_size(cola_ready)||list_size(cola_ready_IOBF)||list_size(cola_ready_VRR)){
				pthread_mutex_lock(&mx_PCP);
				ejecutarPCP(EJECUTAR_PROCESO,NULL);
				pthread_mutex_unlock(&mx_PCP);
			}
			pthread_mutex_unlock(&lock_CPUs);
		}//Fin switch
	}//Fin While
}

int wait_sem(t_semaforo* semaforo, int dtb_id,char* clave){

	semaforo->valor--;
	if(semaforo->valor<0){
		//respuesta negativa al cpu
		list_add(semaforo->cola_bloqueados,(void*)dtb_id);

		dictionary_put(claves,clave,semaforo);
		return -1;

	}
	else{
		//enviar respuesta positiva al cpu
		dictionary_put(claves,clave,semaforo);
		return WAIT_EXITOSO;
	}

}

int signal_sem(t_semaforo* semaforo,char* clave){
	int id_dtb=-1;
	if(semaforo->valor<0){
		id_dtb=(int)list_remove(semaforo->cola_bloqueados,0);
	}
	semaforo->valor++;
	dictionary_put(claves,clave,semaforo);
	return id_dtb;
}


