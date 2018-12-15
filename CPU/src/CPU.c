#include "CPU.h"

int main(){

	//Creo un log para el CPU

	log_CPU = log_create("CPU.log","CPU",false,LOG_LEVEL_INFO);

	file_CPU=config_create("src/CONFIG_CPU.cfg");

	config_CPU.ip_safa=string_duplicate(config_get_string_value(file_CPU,"IP_SAFA"));
	config_CPU.puerto_safa=config_get_int_value(file_CPU,"PUERTO_SAFA");
	config_CPU.ip_diego=string_duplicate(config_get_string_value(file_CPU,"IP_DIEGO"));
	config_CPU.puerto_diego=config_get_int_value(file_CPU,"PUERTO_DIEGO");
	config_CPU.ip_fm9=string_duplicate(config_get_string_value(file_CPU,"IP_FM9"));
	config_CPU.puerto_fm9=config_get_int_value(file_CPU,"PUERTO_FM9");
	config_CPU.retardo=config_get_int_value(file_CPU,"RETARDO");

	config_destroy(file_CPU);
/*
	printf("ip safa: %s\tpuerto safa: %d\tip diego: %s\tpuerto diego: %d\tretardo: %d\n",
				config_CPU.ip_safa,config_CPU.puerto_safa,config_CPU.ip_diego,config_CPU.puerto_diego,config_CPU.retardo);
*/

	int SAFA_fd,DAM_fd,FM9_fd;

	crearSocket(&SAFA_fd);
	crearSocket(&DAM_fd);
	crearSocket(&FM9_fd);

	//El CPU se conecta con SAFA
	if(conectar(&SAFA_fd,config_CPU.puerto_safa,config_CPU.ip_safa)!=0){
		log_error(log_CPU,"Error al conectarse con SAFA");
		exit(1);
	}

	log_info(log_CPU,"Conexion exitosa con SAFA");


	//El CPU se conecta con "el diego"
	if(conectar(&DAM_fd,config_CPU.puerto_diego,config_CPU.ip_diego)!=0){
		log_error(log_CPU,"Error al conectarse con DAM");
		exit(1);
	}

	log_info(log_CPU,"Conexion exitosa con DAM");

	if(conectar(&FM9_fd,config_CPU.puerto_fm9,config_CPU.ip_fm9)!=0){
		log_error(log_CPU,"Error al conectarse con FM9");
		exit(1);
	}

	log_info(log_CPU,"Conexion exitosa con FM9");

	while(1){
		log_info(log_CPU,"Esperando recibir algun DTB/Dummy");
		if(recv(SAFA_fd,&rafaga_recibida,sizeof(int),0)<=0){
			perror("Error al recibir la rafaga de planificacion");
			exit(1);
		}
		//Espero para recibir un DTB a ejecutar

		t_DTB dtb=RecibirYDeserializarDTB(SAFA_fd);

		notificarSAFA(SAFA_fd,ID_DTB,dtb);

		log_info(log_CPU,"DTB Recibido con ID: %d",dtb.id);

		rafaga_actual=rafaga_recibida;

		if(dtb.f_inicializacion==0){
			log_info(log_CPU,"El DTB tiene su flag en 0, comenzando inicializacion...");
			inicializarDTB(DAM_fd,SAFA_fd,&dtb);
		}else{
			log_info(log_CPU,"El DTB tiene su flag en 1, comenzando con la ejecucion de su script...");
			comenzarEjecucion(SAFA_fd,DAM_fd,FM9_fd,dtb);
		}
	}

	return 0;
}

void inicializarDTB(int DAM_fd,int SAFA_fd,t_DTB* dtb){

	void*buffer;
	int protocolo=BLOQUEAR_PROCESO;
	//Enviar peticion de "abrir" al MDJ por medio de "el diego"
	desbloqueo_dummy dummy = {.path = string_duplicate(dtb->escriptorio), .id_dtb = dtb->id};
	serializarYEnviar(DAM_fd,DESBLOQUEAR_DUMMY,&dummy);

	log_info(log_CPU, "Se notifico a DAM para que inicialice el DTB");
	//Una vez enviando el dummy a DAM tengo que avisarle a SAFA que bloquee el proceso mientras se carga en memoria el script

	notificarSAFA(SAFA_fd,protocolo,*dtb);

	usleep(config_CPU.retardo*1000);

}

void comenzarEjecucion(int SAFA, int DAM, int FM9, t_DTB dtb){

	/*
	 * Primero tengo que pedirle al fm9 las lineas para ejecutar
	 * Una vez que recibo las lineas, tengo que parsearlas con mi parser
	 * Luego comunicarme con DAM y enviarle algo dependiendo de la instruccion que se este ejecutando
	 * En caso de ser una I/O debo avisar a SAFA y desalojar el DTB para poder ejecutar otro
	 * Por cada linea que se ejecuta debo decrementar la rafaga_actual, cuando esta llegue a 0 hay que informar a SAFA
	*/

	int interrupcion,uso_DAM=0,isComent;

	direccion_logica* direccion=malloc(sizeof(direccion_logica));

	int protocolo, posicion_file, tamanio_string;

	char* linea_fm9;

	if(dtb.quantum_sobrante!=0){
		rafaga_actual=dtb.quantum_sobrante;
	}


	do{

		interrupcion=0;
		isComent=0;
		direccion->pid=dtb.id;
		direccion->base=0;
		direccion->offset=dtb.pc;

		serializarYEnviar(FM9,PEDIR_LINEA,direccion);

		linea_fm9 = recibirYDeserializarString(FM9);

		if(linea_fm9==NULL){
			log_error(log_CPU,"Se perdio la conexion con FM9, finalizando CPU");
			exit(0);
		}

		t_operacion linea_parseada=parseLine(linea_fm9);

		switch(linea_parseada.keyword){
		case ABRIR:
			log_info(log_CPU,"Se ejecuto la operacion ABRIR");

			actualizarDTB(&dtb);
			if(isOpenFile(dtb,linea_parseada.argumentos.abrir.path)>-1){
				log_info(log_CPU,"El archivo ya esta abierto");
			}
			else{
				protocolo=ABRIR_ARCHIVO;
				serializarYEnviarEntero(DAM,&protocolo);
				serializarYEnviarString(DAM,linea_parseada.argumentos.abrir.path);

				serializarYEnviarEntero(DAM,&dtb.id);

				log_info(log_CPU,"Informacion enviada a DAM para abrir el archivo");

				notificarSAFA(SAFA,SENTENCIA_DAM,dtb);
				uso_DAM=1;

				notificarSAFA(SAFA,BLOQUEAR_PROCESO,dtb);
				interrupcion=1;
			}
			break;
		case CONCENTRAR:
			log_info(log_CPU,"Se ejecuto la operacion CONCENTRAR");

			actualizarDTB(&dtb);
			break;
		case ASIGNAR:
			//CPU se comunica directamente con FM9
			log_info(log_CPU,"Se ejecuto la operacion ASIGNAR");

			cargar_en_memoria* escritura = malloc(sizeof(cargar_en_memoria));

			posicion_file=isOpenFile(dtb,linea_parseada.argumentos.asignar.path);
			int response;

			//archivo abierto
			if(posicion_file!=-1){
				log_info(log_CPU,"El archivo se encuentra abierto por el dtb");
				t_archivo* archivo = list_get(dtb.archivos,posicion_file);

				escritura->id_segmento=archivo->acceso;
				escritura->linea=string_duplicate(linea_parseada.argumentos.asignar.valor);
				escritura->offset=linea_parseada.argumentos.asignar.linea-1;
				escritura->pid=dtb.id;

				log_info(log_CPU,"Se envio la informacion necesaria a FM9 para la asignacion");
				serializarYEnviar(FM9,ESCRIBIR_LINEA,escritura);

				response=*recibirYDeserializarEntero(FM9);

				if(response==LINEA_CARGADA){
					log_info(log_CPU,"Se cargo correctamente la linea en FM9");

				}else{
					log_error(log_CPU,"Ocurrio un error al cargar linea, error segmento/memoria");

					log_info(log_CPU,"Avisando a memoria para que libere las estructuras");

					protocolo=CERRAR_PID;
					serializarYEnviarEntero(FM9,&protocolo);
					serializarYEnviarEntero(FM9,&dtb.id);

					interrupcion=1;

					response=ERROR_SEG_MEM;
				}
			}else{
				log_error(log_CPU,"El archivo sobre el que se desea operar no se encuentra abierto");


				log_info(log_CPU,"Avisando a memoria para que libere las estructuras");

				protocolo=CERRAR_PID;
				serializarYEnviarEntero(FM9,&protocolo);
				serializarYEnviarEntero(FM9,&dtb.id);

				interrupcion=1;

				response=ERROR_ARCHIVO_INEXISTENTE;
			}
			actualizarDTB(&dtb);

			if(interrupcion==1)
				notificarSAFA(SAFA,response,dtb);

			free(escritura->linea);
			free(escritura);

			break;
		case WAIT:
		{
			log_info(log_CPU,"Se ejecuto la operacion WAIT");

			actualizarDTB(&dtb);
			notificarSAFA(SAFA,WAIT_RECURSO,dtb);
			serializarYEnviarString(SAFA,linea_parseada.argumentos.wait.recurso);
			int* resultado_wait=recibirYDeserializarEntero(SAFA);
			if(*resultado_wait!=WAIT_EXITOSO){
				log_warning(log_CPU,"El recurso esta bloqueado");
				interrupcion=1;
			}else{
				log_info(log_CPU,"El recurso se bloqueo/creo con exito");
			}
			free(resultado_wait);
			break;
		}
		case SIGNAL:
		{
			log_info(log_CPU,"Se ejecuto la operacion SIGNAL");

			actualizarDTB(&dtb);
			notificarSAFA(SAFA,SIGNAL_RECURSO,dtb);
			serializarYEnviarString(SAFA,linea_parseada.argumentos.signal.recurso);
			int* resultado_signal=recibirYDeserializarEntero(SAFA);
			if(*resultado_signal!=SIGNAL_EXITOSO){
				interrupcion=1;
			}
			log_info(log_CPU,"Signal realizado con exito");
			free(resultado_signal);
			break;
		}
		case FLUSH:
			//CPU envia una peticion de FLUSH a DAM (envio el protocolo - path - id_dtb)

			log_info(log_CPU,"Se ejecuto la operacion FLUSH");

			actualizarDTB(&dtb);
			if(isOpenFile(dtb,linea_parseada.argumentos.flush.path)!=-1){

				log_info(log_CPU,"El archivo %s se encuentra abierto por el dtb",linea_parseada.argumentos.flush.path);
				direccion_logica* direccion_flush= malloc(sizeof(direccion_logica));
				direccion_flush->pid=dtb.id;
				direccion_flush->offset=0;
				direccion_flush->base=getAccesoFile(dtb,linea_parseada.argumentos.flush.path);

				serializarYEnviar(DAM,FLUSH_ARCHIVO,direccion_flush);
				serializarYEnviarString(DAM,linea_parseada.argumentos.flush.path);

				log_info(log_CPU,"Informacion enviada a DAM para persistir el archivo en MDJ");

				notificarSAFA(SAFA,SENTENCIA_DAM,dtb);
				uso_DAM=1;
				notificarSAFA(SAFA,BLOQUEAR_PROCESO,dtb);

				free(direccion_flush);

			}else{

				log_error(log_CPU,"El dtb no contiene el archivo %s abierto",linea_parseada.argumentos.flush.path);

				log_info(log_CPU,"Avisando a memoria para que libere las estructuras");

				protocolo=CERRAR_PID;
				serializarYEnviarEntero(FM9,&protocolo);
				serializarYEnviarEntero(FM9,&dtb.id);

				notificarSAFA(SAFA,ERROR_ARCHIVO_INEXISTENTE,dtb);

			}

			interrupcion=1;
			break;
		case CLOSE:
			log_info(log_CPU,"Se ejecuto la operacion CLOSE");

			actualizarDTB(&dtb);
			if((posicion_file=isOpenFile(dtb,linea_parseada.argumentos.close.path))!=-1){
				direccion_logica* file = malloc(sizeof(direccion_logica));
				file->pid=dtb.id;
				file->offset=0;
				file->base=getAccesoFile(dtb,linea_parseada.argumentos.close.path);

				//Aca va lo de enviar la direccion al FM9

				serializarYEnviar(FM9,CERRAR_ARCHIVO,file);

				response=*recibirYDeserializarEntero(FM9);

				if(response==FINAL_CERRAR){
					log_info(log_CPU,"El archivo fue cerraado con exito");
					//Actualizo la info del dtb (quito el archivo de su lista)
					list_remove(dtb.archivos,posicion_file);
				}else{
					log_error(log_CPU,"Ocurrio un error al cerrar el archivo, error segmento/memoria");

					log_info(log_CPU,"Avisando a memoria para que libere las estructuras");

					protocolo=CERRAR_PID;
					serializarYEnviarEntero(FM9,&protocolo);
					serializarYEnviarEntero(FM9,&dtb.id);

					interrupcion=1;
				}
				free(file);
			}else{
				log_error(log_CPU,"El dtb intento cerrar un archivo que no tiene abierto");

				log_info(log_CPU,"Avisando a memoria para que libere las estructuras");

				protocolo=CERRAR_PID;
				serializarYEnviarEntero(FM9,&protocolo);
				serializarYEnviarEntero(FM9,&dtb.id);

				response=ERROR_ARCHIVO_INEXISTENTE;

				interrupcion=1;
			}
			notificarSAFA(SAFA,response,dtb);
			break;
		case CREAR:
		{
			log_info(log_CPU,"Se ejecuto la operacion CREAR");
			peticion_crear* crear_archivo=malloc(sizeof(peticion_crear));
			crear_archivo->path=string_duplicate(linea_parseada.argumentos.crear.path);
			crear_archivo->cant_lineas=linea_parseada.argumentos.crear.lineas;
			serializarYEnviar(DAM,CREAR_ARCHIVO,crear_archivo);
			serializarYEnviarEntero(DAM,&dtb.id);

			log_info(log_CPU,"Informacion enviada a DAM para Crear el archivo");

			notificarSAFA(SAFA,SENTENCIA_DAM,dtb);
			uso_DAM=1;
			actualizarDTB(&dtb);
			notificarSAFA(SAFA,BLOQUEAR_PROCESO,dtb);
			interrupcion=1;

			free(crear_archivo->path);
			free(crear_archivo);
			break;
		}
		case BORRAR:
			//CPU envia peticion de borrar un archivo a DAM (protocolo - path - id_dtb)
			log_info(log_CPU,"Se ejecuto la operacion BORRAR");

			peticion_borrar* borrar_archivo=malloc(sizeof(peticion_borrar));
			borrar_archivo->path=string_duplicate(linea_parseada.argumentos.borrar.path);
			serializarYEnviar(DAM,BORRAR_ARCHIVO,borrar_archivo);
			serializarYEnviarEntero(DAM,&dtb.id);

			log_info(log_CPU,"Informacion enviada a DAM para Borrar el archivo");

			notificarSAFA(SAFA,SENTENCIA_DAM,dtb);
			uso_DAM=1;
			actualizarDTB(&dtb);
			notificarSAFA(SAFA,BLOQUEAR_PROCESO,dtb);
			interrupcion=1;

			free(borrar_archivo->path);
			free(borrar_archivo);
			break;
		case FIN:
			log_info(log_CPU,"Se termino de ejecutar el script");

			log_info(log_CPU,"Avisando a memoria para que libere las estructuras");

			protocolo=CERRAR_PID;
			serializarYEnviarEntero(FM9,&protocolo);
			serializarYEnviarEntero(FM9,&dtb.id);

			notificarSAFA(SAFA,FINALIZAR_PROCESO,dtb);
			interrupcion=1;
			break;
		case COMENT:
			isComent=1;
			break;
		}

		if(!isComent){
		if(rafaga_recibida!=0){
			if(rafaga_actual==0 && interrupcion==0){
				log_warning(log_CPU,"El DTB %d se quedo sin quantum",dtb.id);
				notificarSAFA(SAFA,FIN_QUANTUM,dtb);
				interrupcion=1;
			}
		}
		if(linea_parseada.keyword!=FIN)
			destroyParse(linea_parseada);

		log_warning(log_CPU, "Instruccion ejecutada, quantum restante = %d",dtb.quantum_sobrante);

		if(!uso_DAM&&!interrupcion)
			notificarSAFA(SAFA,SENTENCIA_EJECUTADA,dtb);
		}
		usleep(config_CPU.retardo*1000);
	}while(!interrupcion);

	log_warning(log_CPU,"Se produjo una interrupcion durante la ejecucion del dtb %d",dtb.id);

	free(linea_fm9);
	free(direccion);
}

void notificarSAFA(int SAFA,int protocolo, t_DTB DTB){

	void*buffer;

	int id_dtb=DTB.id;

	send(SAFA,&protocolo,sizeof(int),0);

	if(protocolo==ID_DTB)
		serializarYEnviarEntero(SAFA,&id_dtb);
	else if(protocolo!=SENTENCIA_DAM)
		serializarYEnviarDTB(SAFA,buffer,DTB);

}

void actualizarDTB(t_DTB* dtb){

	if(rafaga_recibida!=0){
		rafaga_actual--;

		dtb->quantum_sobrante=rafaga_actual;
	}
	dtb->pc++;

}

int isOpenFile(t_DTB dtb, char* path){

	int i, retorno = -1;

	int cant_archivos = list_size(dtb.archivos);

	t_archivo* archivo;

	for(i=0;i<cant_archivos;i++){

		archivo=list_get(dtb.archivos,i);

		if(strcmp(archivo->path,path)==0){
			retorno = i;
			break;
		}

	}

	return retorno;
}

int getAccesoFile(t_DTB dtb, char* path){

	int cant_archivos = list_size(dtb.archivos);

	int i,retorno;

	t_archivo* archivo;

	for(i=0;i<cant_archivos;i++){

		archivo=list_get(dtb.archivos,i);

		if(strcmp(archivo->path,path)==0){
			retorno = archivo->acceso;
			break;
		}

	}

	return retorno;
}


