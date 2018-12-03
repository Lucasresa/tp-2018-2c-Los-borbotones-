#include "S-AFA.h"

int main(){

	//flag para saber si la consola esta operativa

	int flag_consola=0;

	log_SAFA=log_create("log_SAFA.log","SAFA",true,LOG_LEVEL_INFO);

	CPU_libres=list_create();

	claves=dictionary_create();

	//Creo las colas del S-AFA

	crear_colas();

	info_metricas=list_create();

	//Levanto archivo de configuracion del S-AFA e inicializo los semaforos

	iniciar_semaforos();

	load_config();

	multiprogramacion_actual=config_SAFA.multiprog;


	//Hilo para actualizar achivo de configuracion del S-AFA

	pthread_t hilo_inotify;

	pthread_create(&hilo_inotify,NULL,(void*)actualizar_file_config,NULL);

	pthread_detach(hilo_inotify);

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

	//Tiro un hilo para ejecutar la consola

		if(flag_consola==0){
			flag_consola=1;

			pthread_t hiloConsola;

			log_info(log_SAFA,"Consola en linea...");

			pthread_create(&hiloConsola,NULL,(void*)consola_SAFA,NULL);

			pthread_detach(hiloConsola);

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

	while(1){

		dtb=malloc(sizeof(t_DTB));
		dtb->archivos=list_create();

		protocolo=recibirYDeserializarEntero(fd_DAM);

		if(protocolo==NULL){
			log_error(log_SAFA,"Se perdio la conexion con DAM, cierro SAFA");
			exit(0);
		}

		switch(*protocolo){
		case FINAL_CARGA_DUMMY:
			dtb->id=*recibirYDeserializarEntero(fd_DAM);

			pthread_mutex_lock(&mx_PCP);
			ejecutarPCP(DESBLOQUEAR_DUMMY,dtb);
			pthread_mutex_unlock(&mx_PCP);
			pthread_mutex_lock(&mx_PLP);
			ejecutarPLP();
			pthread_mutex_unlock(&mx_PLP);
			break;
		case FINAL_ABRIR:
		{
			t_archivo* archivo=malloc(sizeof(t_archivo));
			id_dtb=*recibirYDeserializarEntero(fd_DAM);

			pthread_mutex_lock(&mx_colas);
			aux=getDTBEnCola(cola_block,id_dtb);
			pthread_mutex_unlock(&mx_colas);

			archivo->path=recibirYDeserializarString(fd_DAM);
			archivo->acceso=*recibirYDeserializarEntero(fd_DAM);
			list_add(aux->archivos,archivo);
			pthread_mutex_lock(&mx_PCP);
			ejecutarPCP(DESBLOQUEAR_PROCESO,aux);
			pthread_mutex_unlock(&mx_PCP);
			pthread_mutex_lock(&mx_PCP);
			ejecutarPCP(EJECUTAR_PROCESO,NULL);
			pthread_mutex_unlock(&mx_PCP);

			break;
		}
		case FINAL_CREAR:
			dtb->id=*recibirYDeserializarEntero(fd_DAM);
			pthread_mutex_lock(&mx_PCP);
			ejecutarPCP(DESBLOQUEAR_PROCESO,dtb);
			pthread_mutex_unlock(&mx_PCP);
			break;
		case FINALIZAR_PROCESO:
			id_dtb=*recibirYDeserializarEntero(fd_DAM);
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
		case ERROR_ARCHIVO_EXISTENTE:
			id_dtb=*recibirYDeserializarEntero(fd_DAM);
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
			id_dtb=*recibirYDeserializarEntero(fd_DAM);
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
			id_dtb=*recibirYDeserializarEntero(fd_DAM);
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
			id_dtb=*recibirYDeserializarEntero(fd_DAM);
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
			id_dtb=*recibirYDeserializarEntero(fd_DAM);
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

	log_info(log_SAFA,"ejecutando PCP...");

	pthread_mutex_lock(&mx_PCP);
	ejecutarPCP(EJECUTAR_PROCESO,NULL);
	pthread_mutex_unlock(&mx_PCP);

	int dtb_cpu;

	while(1){
		if(recv(fd_CPU,&protocolo,sizeof(int),0)<=0){

			log_info(log_SAFA,"Se desconecto el CPU %d",fd_CPU);


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
				log_info(log_SAFA,"El CPU %d estaba libre, borrando de la lista....",fd_CPU);
				pthread_mutex_lock(&mx_CPUs);
				eliminarSocketCPU(fd_CPU);
				pthread_mutex_lock(&mx_CPUs);
			}

			log_info(log_SAFA,"Se elimino el CPU %d de la lista de CPUs",fd_CPU);

			pthread_exit(NULL);
		}
	//Me fijo que peticion esta haciendo la CPU dependiendo del protocolo que envio
		t_DTB* dtb;
		t_DTB dtb_modificado;
		log_info(log_SAFA,"protocolo: %d",protocolo);

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
		if(protocolo==WAIT_RECURSO){
			clave=recibirYDeserializarString(fd_CPU);

			if(dictionary_has_key(claves,clave)){ //Si existe la clave en el sistema
				pthread_mutex_lock(&mx_claves);
				t_semaforo* sem_clave=dictionary_remove(claves,clave);
				pthread_mutex_unlock(&mx_claves);

				respuesta=wait_sem(sem_clave,dtb_modificado.id,clave);

				if(respuesta==-1){
					pthread_mutex_lock(&mx_colas);
					dtb=dictionary_remove(cola_exec,string_itoa(dtb_modificado.id));
					pthread_mutex_unlock(&mx_colas);

					pthread_mutex_lock(&mx_PCP);
					ejecutarPCP(BLOQUEAR_PROCESO,dtb);
					pthread_mutex_unlock(&mx_PCP);
					//El CPU queda libre para ejecutar otro proceso
					pthread_mutex_lock(&mx_CPUs);
					list_add(CPU_libres,(void*)fd_CPU);
					pthread_mutex_unlock(&mx_CPUs);
					//Vuelvo a ejecutar el planificador pero esta vez para que, de ser posible, asigne otro proceso al CPU
					pthread_mutex_lock(&mx_PCP);
					ejecutarPCP(EJECUTAR_PROCESO,NULL);
					pthread_mutex_unlock(&mx_PCP);
				}
			}else{ //Si no existe la clave en el sistema, tengo que generarla
				t_semaforo* clave_nueva=malloc(sizeof(t_semaforo));
				clave_nueva->valor=1;
				clave_nueva->cola_bloqueados=list_create();

				respuesta=wait_sem(clave_nueva,dtb_modificado.id,clave);
			}
			serializarYEnviarEntero(fd_CPU,&respuesta);
		//Si recibe un signal de algun recurso
		}else if(protocolo==SIGNAL_RECURSO){
			clave=recibirYDeserializarString(fd_CPU);

			if(dictionary_has_key(claves,clave)){ //Si existe la clave en el sistema
				t_semaforo* sem_clave=dictionary_remove(claves,clave);

				log_info(log_SAFA,"Clave %s - Procesos bloqueados %d - id dtb bloqueados %d",clave,sem_clave->valor,(int)list_get(sem_clave->cola_bloqueados,0));

				respuesta=signal_sem(sem_clave,clave);

				if(respuesta!=-1){
					dtb=malloc(sizeof(t_DTB));
					dtb->id=respuesta;
					pthread_mutex_lock(&mx_PCP);
					ejecutarPCP(DESBLOQUEAR_PROCESO,dtb);
					pthread_mutex_unlock(&mx_PCP);
					free(dtb);
				}
			}else{ //Si no existe la clave en el sistema, tengo que generarla
				t_semaforo* clave_nueva = malloc(sizeof(t_semaforo));
				clave_nueva->valor=1;
				clave_nueva->cola_bloqueados=list_create();
				pthread_mutex_lock(&mx_claves);
				dictionary_put(claves,clave,clave_nueva);
				pthread_mutex_unlock(&mx_claves);
			}
			respuesta=SIGNAL_EXITOSO;
			serializarYEnviarEntero(fd_CPU,&respuesta);

		}else if(protocolo==ID_DTB){
			dtb_cpu=*recibirYDeserializarEntero(fd_CPU);
			log_info(log_SAFA,"Recibido el id del dtb que ejecuta el CPU, %d",dtb_cpu);
		}else if(protocolo==SENTENCIA_EJECUTADA){
			log_info(log_SAFA,"Actualizando metricas del DTB %d",dtb_cpu);

			actualizarMetricaDTB(dtb_cpu,protocolo);
			actualizarMetricasDTBNew();
		}else if(protocolo==SENTENCIA_DAM){
			log_info(log_SAFA,"Actualizando metricas del DTB %d",dtb_cpu);
			actualizarMetricaDTB(dtb_cpu,protocolo);
			actualizarMetricasDTBNew();
		}
		else{

			pthread_mutex_lock(&mx_colas);
			dtb=dictionary_remove(cola_exec,string_itoa(dtb_modificado.id));
			pthread_mutex_unlock(&mx_colas);

			//Ejecuto el planificador para que decida que hacer con el dtb dependiendo del protocolo recibido
			pthread_mutex_lock(&mx_PCP);
			ejecutarPCP(protocolo,dtb);
			pthread_mutex_unlock(&mx_PCP);

			if(protocolo==FINALIZAR_PROCESO&&dtb->f_inicializacion==1){
				pthread_mutex_lock(&mx_PLP);
				ejecutarPLP();
				pthread_mutex_unlock(&mx_PLP);
			}

			//El CPU queda libre para ejecutar otro proceso
			pthread_mutex_lock(&mx_CPUs);
			list_add(CPU_libres,(void*)fd_CPU);
			pthread_mutex_unlock(&mx_CPUs);

			//Vuelvo a ejecutar el planificador pero esta vez para que, de ser posible, asigne otro proceso al CPU
			pthread_mutex_lock(&mx_PCP);
			ejecutarPCP(EJECUTAR_PROCESO,NULL);
			pthread_mutex_unlock(&mx_PCP);
		}
	}
}

int wait_sem(t_semaforo* semaforo, int dtb_id,char* clave){

	semaforo->valor--;
	if(semaforo->valor<0){
		//respuesta negativa al cpu
		list_add(semaforo->cola_bloqueados,(void*)dtb_id);

		pthread_mutex_lock(&mx_claves);
		dictionary_put(claves,clave,semaforo);
		pthread_mutex_unlock(&mx_claves);
		return -1;

	}
	else{
		//enviar respuesta positiva al cpu
		pthread_mutex_lock(&mx_claves);
		dictionary_put(claves,clave,semaforo);
		pthread_mutex_unlock(&mx_claves);
		return WAIT_EXITOSO;
	}

}

int signal_sem(t_semaforo* semaforo,char* clave){
	int id_dtb=-1;
	if(semaforo->valor<0){
		log_info(log_SAFA,"Hay %d dtbs bloqueados, desbloqueo el primero",list_size(semaforo->cola_bloqueados));
		id_dtb=(int)list_remove(semaforo->cola_bloqueados,0);
	}
	semaforo->valor++;
	pthread_mutex_lock(&mx_claves);
	dictionary_put(claves,clave,semaforo);
	pthread_mutex_unlock(&mx_claves);
	return id_dtb;
}


