#include "S-AFA.h"

int main(){
	//flag para saber si la consola esta operativa
	int flag_consola=0;

	log_SAFA=log_create("log_SAFA.log","SAFA",true,LOG_LEVEL_INFO);

	CPU_libres=list_create();

	//Creo las colas del S-AFA

	crear_colas();

	//Levanto archivo de configuracion del S-AFA

	load_config();

	//Hilo para actualizar achivo de configuracion del S-AFA

	pthread_t hilo_notify;

	pthread_create(&hilo_notify,NULL,(void*)actualizar_file_config,NULL);

	//-------------------------------------------------------------------------------------------------------

	//Aca habria que hacer la parte de la conexion con los demas procesos usando sockets. (Usando la shared library de sockets

	int SAFA_fd,DAM_fd;
	crearSocket(&SAFA_fd);

	setearParaEscuchar(&SAFA_fd,config_SAFA.port);

	log_info(log_SAFA,"Escuchando nuevas conexiones....");

/*	DAM_fd=aceptarConexion(SAFA_fd);

	if(DAM_fd==-1){
		perror("Error de conexion con DAM");
		log_destroy(log_SAFA);
		exit(1);
	}

	log_info(log_SAFA,"Conexion exitosa con DAM");

	pthread_t hiloDAM;

	pthread_create(&hiloDAM,NULL,(void*)atenderDAM,(void*)&DAM_fd);

	pthread_detach(hiloDAM);
*/

	//El hilo main se queda esperando que se conecten nuevas CPU

	while(1){

		pthread_t hiloCPU;
		int CPU_fd;

		if((CPU_fd=aceptarConexion(SAFA_fd))!=-1){


			pthread_create(&hiloCPU,NULL,(void*)atenderCPU,(void*)&CPU_fd);
			pthread_detach(hiloCPU);
			log_info(log_SAFA,"Conexion exitosa con la CPU %d",CPU_fd);

			list_add(CPU_libres,(void*)CPU_fd);


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

}

void atenderCPU(int*fd){

	int fd_CPU = *fd;
	void*buffer=malloc(1);

	log_info(log_SAFA,"Enviando info del quantum al CPU %d...",fd_CPU);

	send(fd_CPU,&config_SAFA.quantum,sizeof(int),0);

	if(queue_size(cola_ready)!=0){

		log_info(log_SAFA,"Hay %d procesos esperando en Ready, ejecutando PCP",queue_size(cola_ready));

		ejecutarPCP();

	}

	if(recv(fd_CPU,buffer,1,0)<=0){

		log_info(log_SAFA,"Se desconecto el CPU %d",fd_CPU);

		pthread_mutex_lock(&bloqueo_CPU);

		//Si el CPU estaba ejecutando un proceso, este se envia a exit y se ejecutar el PLP para que replanifique
		if(dictionary_has_key(cola_exec,string_itoa(fd_CPU))){
			t_DTB* dtb=dictionary_remove(cola_exec,string_itoa(fd_CPU));
			log_info(log_SAFA,"El CPU %d estaba ejecutando el proceso %d finalizando....",fd_CPU,dtb->id);
			queue_push(cola_exit,dtb);
			config_SAFA.multiprog+=1;
			ejecutarPLP();
		}
		else{ //En caso de no estar ejecutando nada, lo elimino de la lista de CPU_libres
			log_info(log_SAFA,"El CPU %d estaba libre, borrando de la lista....",fd_CPU);
			eliminarSocketCPU(fd_CPU);
		}
		pthread_mutex_unlock(&bloqueo_CPU);

		log_info(log_SAFA,"Se elimino el CPU %d de la lista de CPUs",fd_CPU);

		free(buffer);
	}

}

