#include "DAM.h"

int main(){

	log_DAM = log_create("DAM.log","DAM",true,LOG_LEVEL_INFO);

    char *archivo;
	archivo="src/CONFIG_DAM.cfg";
    if(validarArchivoConfig(archivo) <0)
    	return -1;

	file_DAM = config_create(archivo);

	pthread_mutex_init(&mutex_SAFA,NULL);

	config_DAM.puerto_dam=config_get_int_value(file_DAM,"PUERTO");
	config_DAM.ip_safa=string_duplicate(config_get_string_value(file_DAM,"IP_SAFA"));
	config_DAM.puerto_safa=config_get_int_value(file_DAM,"PUERTO_SAFA");
	config_DAM.ip_mdj=string_duplicate(config_get_string_value(file_DAM,"IP_MDJ"));
	config_DAM.puerto_mdj=config_get_int_value(file_DAM,"PUERTO_MDJ");
	config_DAM.ip_fm9=string_duplicate(config_get_string_value(file_DAM,"IP_FM9"));
	config_DAM.puerto_fm9=config_get_int_value(file_DAM,"PUERTO_FM9");
	config_DAM.transfer_size=config_get_int_value(file_DAM,"TRANSFER_SIZE");

	config_destroy(file_DAM);

	crearSocket(&SAFA_fd);
	crearSocket(&MDJ_fd);
	crearSocket(&FM9_fd);

	int listener_socket;
	crearSocket(&listener_socket);

	setearParaEscuchar(&listener_socket, config_DAM.puerto_dam);

	log_info(log_DAM, "Escuchando conexiones...");

	FD_ZERO(&set_fd);
	FD_SET(listener_socket, &set_fd);

	//El DAM se conecta con MDJ
	if(conectar(&MDJ_fd,config_DAM.puerto_mdj,config_DAM.ip_mdj)!=0){
		log_error(log_DAM,"Error al conectarse con MDJ");
		exit(1);
	}
	else{
		log_info(log_DAM,"Conexion con MDJ establecida");
	}

	//El DAM se conecta con FM9_fd
	if(conectar(&FM9_fd,config_DAM.puerto_fm9,config_DAM.ip_fm9)!=0){
		log_error(log_DAM,"Error al conectarse con FM9");
		exit(1);
	} else {
		log_info(log_DAM, "Conexión con FM9 establecido");
	}

	//El DAM se conecta con SAFA
	if(conectar(&SAFA_fd,config_DAM.puerto_safa,config_DAM.ip_safa)!=0){
		log_error(log_DAM,"Error al conectarse con SAFA");
		exit(1);
	} else {
		log_info(log_DAM, "Conexión con SAFA establecido");
		pthread_t hilo_SAFA;
		pthread_create(&hilo_SAFA,NULL,(void*)escucharSAFA,(void*)&SAFA_fd);
		log_info(log_DAM,"Hilo para escuchar mensajes de SAFA creado");
		pthread_detach(hilo_SAFA);
	}

	while(true) {
		escuchar(listener_socket, &set_fd, &funcionHandshake, NULL, &recibirPeticion, NULL );
	}

	return 0;
}

void* funcionHandshake(int socket, void* argumentos) {
	log_info(log_DAM, "Conexión establecida");
	return 0;
}

void* recibirPeticion(int socket, void* argumentos) {
	int* header,success;

	header = recibirYDeserializarEntero(socket);

	if(header==NULL){
		log_info(log_DAM,"Se perdio la conexion con el socket %d",socket);
		close(socket);
	}


	switch(*header){
	case DESBLOQUEAR_DUMMY:
	{
		desbloqueo_dummy* dummy;
		dummy = recibirYDeserializar(socket,*header);
		log_info(log_DAM,"Recibi el header desbloquear dummy %s", dummy->path);

		if(validarArchivoMDJ(MDJ_fd,dummy->path)>0){

			log_info(log_DAM,"Validacion exitosa en MDJ");

			char* buffer = obtenerArchivoMDJ(dummy->path);

			log_info(log_DAM,"Cargo archivo al FM9");
			cargarScriptFM9(dummy->id_dtb, buffer);
			log_info(log_DAM,"Enviando final carga dummy");
			success=FINAL_CARGA_DUMMY;

		    log_info(log_DAM,"Final carga dummy enviado al safa");

		    free(buffer);
		}
		else{
			log_error(log_DAM,"El escriptorio no existe en MDJ");

			success=ERROR_ARCHIVO_INEXISTENTE;

			log_info(log_DAM,"Se envio un error a SAFA para que aborte el DTB dummy");
		}
		pthread_mutex_lock(&mutex_SAFA);
		serializarYEnviarEntero(SAFA_fd,&success);
	    serializarYEnviarEntero(SAFA_fd,&dummy->id_dtb);
		pthread_mutex_unlock(&mutex_SAFA);

		break;
	}
	case CREAR_ARCHIVO:
	{
		log_info(log_DAM,"Peticion de crear archivo recibida");
		peticion_crear* crear_archivo=recibirYDeserializar(socket,*header);
		int dtb_id = *recibirYDeserializarEntero(socket);

		log_info(log_DAM,"Validando que el archivo %s no exista",crear_archivo->path);

		if(validarArchivoMDJ(MDJ_fd,crear_archivo->path)<0){
			log_info(log_DAM,"El archivo no existe, sigo con la creacion");

			if(crearArchivoMDJ(MDJ_fd,*header,crear_archivo)){
				log_info(log_DAM,"Se creo exitosamente el archivo %s", crear_archivo->path);
				success = FINAL_CREAR;
			}else{
				log_error(log_DAM,"No hay espacio suficiente en MDJ para crear el archivo");
				success= ERROR_MDJ_SIN_ESPACIO;
			}
		}else{
			log_error(log_DAM,"El archivo ya existe en MDJ, Avisando a SAFA...");
			success=ERROR_ARCHIVO_EXISTENTE;
		}
		pthread_mutex_lock(&mutex_SAFA);
		serializarYEnviarEntero(SAFA_fd,&success);
		serializarYEnviarEntero(SAFA_fd,&dtb_id);
		pthread_mutex_unlock(&mutex_SAFA);
		log_info(log_DAM,"Se le informo a SAFA el resultado de la operacion");
		free(crear_archivo->path);
		free(crear_archivo);
		break;
	}
	case ABRIR_ARCHIVO:
	{
		log_info(log_DAM,"Peticion de abrir archivo recibida");

		char*path = recibirYDeserializarString(socket);
		int dtb_id = *recibirYDeserializarEntero(socket);

		if(validarArchivoMDJ(MDJ_fd,path)>0){
			char* buffer = obtenerArchivoMDJ(path);

			log_info(log_DAM,"Cargo archivo al FM9");
			int base = cargarArchivoFM9(dtb_id, buffer);

			success=FINAL_ABRIR;

			info_archivo info_archivo = {.path=path,.pid=dtb_id,.acceso=base};

			pthread_mutex_lock(&mutex_SAFA);
			serializarYEnviar(SAFA_fd,FINAL_ABRIR,&info_archivo);
			pthread_mutex_unlock(&mutex_SAFA);
			//FALTA VALIDAR SI HAY ESPACIO EN FM9
			log_info(log_DAM,"Se informo a SAFA que el archivo se cargo con exito");

			free(buffer);
		}else{
			log_error(log_DAM,"El archivo NO existe en MDJ, Avisando a SAFA...");

			success=ERROR_ARCHIVO_INEXISTENTE;

			pthread_mutex_lock(&mutex_SAFA);
			serializarYEnviarEntero(SAFA_fd,&success);
			serializarYEnviarEntero(SAFA_fd,&dtb_id);
			pthread_mutex_unlock(&mutex_SAFA);

			log_info(log_DAM,"Se informo a SAFA que el archivo no existe");
		}
		free(path);
	    break;
	}
	case BORRAR_ARCHIVO:
	{
		log_info(log_DAM,"Peticion de borrar archivo recibida");
		peticion_borrar* borrar_archivo=recibirYDeserializar(socket,*header);
		int dtb_id = *recibirYDeserializarEntero(socket);

		log_info(log_DAM,"Validando que el archivo %s exista",borrar_archivo->path);

		if(validarArchivoMDJ(MDJ_fd,borrar_archivo->path)>0){
			log_info(log_DAM,"El archivo existe, procedo a borrarlo");

			serializarYEnviar(MDJ_fd,*header,borrar_archivo);
			success=FINAL_BORRAR;

		}else{
			log_error(log_DAM,"El archivo NO existe en MDJ, Avisando a SAFA...");
			success=ERROR_ARCHIVO_INEXISTENTE;
		}

		pthread_mutex_lock(&mutex_SAFA);
		serializarYEnviarEntero(SAFA_fd,&success);
		serializarYEnviarEntero(SAFA_fd,&dtb_id);
		pthread_mutex_lock(&mutex_SAFA);

		log_info(log_DAM,"Se le informo a SAFA el resultado de la operacion");
		free(borrar_archivo->path);
		free(borrar_archivo);
		break;
	}
	case FLUSH_ARCHIVO:

		log_info(log_DAM,"Peticion de flush sobre un archivo recibida");

		break;
	}
	return 0;
}

int guardarArchivoMDJ(char* path, char* buffer) {
	peticion_guardar guardado = {.path="users/luquitas.txt",.offset=1,.size=20,.buffer="holaholahola"};
	serializarYEnviar(MDJ_fd,GUARDAR_DATOS,&guardado);
	log_info(log_DAM,"Se envio una peticion de guardado al MDJ");
	return 0;
}

char* obtenerArchivoMDJ(char *path) {
	char *enviar = malloc(sizeof(char) * (config_DAM.transfer_size+1));
	int offset_enviar;
	offset_enviar=0;
	while (1){
		peticion_obtener obtener = {.path=path,.offset=offset_enviar,.size=config_DAM.transfer_size};
		log_info(log_DAM,"Enviando peticion al MDJ...");
		serializarYEnviar(MDJ_fd,OBTENER_DATOS,&obtener);
		log_info(log_DAM,"Peticion envada...");
		char *buffer = malloc(sizeof(char) * (config_DAM.transfer_size));
		while(1) {
			buffer = recibirYDeserializarString(MDJ_fd);
			printf("Recibi: %s\n",buffer);
			break;
		}
		if(offset_enviar==0){
			strcpy(enviar,buffer);
		}
		else{
			string_append(&enviar,buffer);
		}
		int size=strlen(buffer);
		if(size != config_DAM.transfer_size){
			break;
		}
		offset_enviar++;

		free(buffer);
	}
	puts(enviar);
	return enviar;
}

int cargarArchivoFM9(int pid, char* buffer) {
	//int contador_offset;
	iniciar_scriptorio_memoria* datos_script = malloc(sizeof(iniciar_scriptorio_memoria));

	datos_script->pid = pid;
    int count = 0;
    char *ptr = buffer;
    while((ptr = strchr(ptr, '\n')) != NULL) {
        count++;
        ptr++;
    }
	datos_script->size_script = count;
	serializarYEnviar(FM9_fd,ABRIR_ARCHIVO,datos_script);
	free(datos_script);

	if (recibirHeader(FM9_fd, ESTRUCTURAS_CARGADAS) != 0) {
		return -1;
	}

	direccion_logica* direccion = recibirYDeserializar(FM9_fd,ESTRUCTURAS_CARGADAS);

    char* linea = NULL;
	linea = strtok(buffer, "\n");
	cargar_en_memoria paquete = {.pid=pid,.id_segmento=direccion->base,.offset=0,.linea=NULL};

	log_info(log_DAM,"Enviando archivo al FM9...\n");

    while (linea != NULL)
    {
    	paquete.linea=linea;
    	serializarYEnviar(FM9_fd,ESCRIBIR_LINEA,&paquete);
        //printf("%i: %s\n", paquete.offset, paquete.linea);
        paquete.offset++;
        linea = strtok(NULL, "\n");
    	if (recibirHeader(FM9_fd, LINEA_CARGADA) != 0) {
    		return -1;
    		log_error(log_DAM,"Error cargando linea %s", linea);
    	}
    }

    return direccion->base;
}

int cargarScriptFM9(int pid, char* buffer) {

	//int contador_offset;
	iniciar_scriptorio_memoria* datos_script = malloc(sizeof(iniciar_scriptorio_memoria));

	datos_script->pid = pid;
    int count = 0;
    char *ptr = buffer;
    while((ptr = strchr(ptr, '\n')) != NULL) {
        count++;
        ptr++;
    }
	datos_script->size_script = count;
	serializarYEnviar(FM9_fd,INICIAR_MEMORIA_PID,datos_script);
	free(datos_script);

	if (recibirHeader(FM9_fd, MEMORIA_INICIALIZADA) != 0) {
		return -1;
	}

    char* linea = NULL;
	linea = strtok(buffer, "\n");
	cargar_en_memoria paquete = {.pid=pid,.id_segmento=0,.offset=0,.linea=NULL};

	log_info(log_DAM,"Enviando archivo al FM9...\n");

    while (linea != NULL)
    {
    	paquete.linea=linea;
    	serializarYEnviar(FM9_fd,ESCRIBIR_LINEA,&paquete);
        //printf("%i: %s\n", paquete.offset, paquete.linea);
        paquete.offset++;
        linea = strtok(NULL, "\n");
    	if (recibirHeader(FM9_fd, LINEA_CARGADA) != 0) {
    		return -1;
    	}
    }

    return 1;
}

int recibirHeader(int socket, int headerEsperado) {
	int headerRecibido;
	int length = recv(socket,&headerRecibido,sizeof(int),0);
	if ( length == -1 ) {
		perror("error");
		log_error(log_DAM, "Socket sin conexión.");
		close(socket);
		return -1;
	}

	if (headerRecibido != headerEsperado) {
		length = recv(socket,&headerRecibido,sizeof(int),0);
		if (headerRecibido != headerEsperado) {
			log_error(log_DAM, "No recibí el header que esperaba. Esperaba %s y recibí %s", headerEsperado, headerRecibido);
			return -1;
		}
	}
	return 0;
}

void testeoFM9() {
	char bufferTesteo[200] = "crear /equipos/equipo1.txt 5\nabrir /equipos/equipo\n";
	log_info(log_DAM,"Cargo archivo al FM9");
	cargarScriptFM9(0, bufferTesteo);
	log_info(log_DAM,"Enviando final carga dummy");

	char bufferTesteoDos[60] = "\n\n\n\n\n";
	cargarArchivoFM9(0, bufferTesteoDos);

	char bufferTesteoTres[200] = "crear /equipos/equipo2.txt 5\nabrir /equipos/equipo2.txt jeje\notra linea\n";
	log_info(log_DAM,"Cargo archivo al FM9");
	cargarScriptFM9(1, bufferTesteoTres);
	log_info(log_DAM,"Enviando final carga dummy");

	char bufferTesteoCuatro[60] = "\n\n\n\n\n";
	cargarArchivoFM9(1, bufferTesteoCuatro);
}

int validarArchivoMDJ(int MDJ, char* path){

	peticion_validar* validacion = malloc(sizeof(peticion_validar));
	validacion->path = string_duplicate(path);
	int header = VALIDAR_ARCHIVO,respuesta,retorno;

	serializarYEnviar(MDJ,header,validacion);

	respuesta = *recibirYDeserializarEntero(MDJ);

	free(validacion->path);
	free(validacion);

	if(respuesta==VALIDAR_OK){
		retorno= 1;
	}else{
		retorno= -1;
	}

	return retorno;
}

int crearArchivoMDJ(int MDJ,int operacion,peticion_crear* crear_archivo){
	log_info(log_DAM,"Enviando peticion de crear archivo a MDJ");
	serializarYEnviar(MDJ_fd,operacion,crear_archivo);
	int respuesta,retorno;

	respuesta = *recibirYDeserializarEntero(MDJ_fd);

	switch(respuesta){
	case CREAR_OK:
		retorno=1;
		break;
	case CREAR_FALLO:
		retorno=0;
		break;
	}
	return retorno;
}

void escucharSAFA(int* fd){
	int SAFA=*fd, id_dtb;

	int*protocolo;

	while(1){

		protocolo=recibirYDeserializarEntero(SAFA);

		pthread_mutex_lock(&mutex_SAFA);

		if(protocolo==NULL){
			log_error(log_DAM,"Se perdio la conexion con SAFA, cierro DAM");
			free(protocolo);
			exit(0);
		}

		switch(*protocolo){
		case FINALIZAR_PROCESO:

			log_info(log_DAM,"SAFA me aviso que un proceso acaba de finalizar");

			id_dtb=*recibirYDeserializarEntero(SAFA);

			//Avisar a FM9 que tiene que liberar el espacio ocupado por el proceso

			break;
		}

		pthread_mutex_unlock(&mutex_SAFA);

	}
}




