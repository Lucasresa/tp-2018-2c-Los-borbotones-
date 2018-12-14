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
	int error_holder = 0;

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
			cargarScriptFM9(dummy->id_dtb, buffer, &error_holder);
			if (error_holder != 0) {
				pthread_mutex_lock(&mutex_SAFA);
				serializarYEnviarEntero(SAFA_fd,&error_holder);
				pthread_mutex_unlock(&mutex_SAFA);
				return (void*)-1;
			}
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
			int base = cargarArchivoFM9(dtb_id, buffer, &error_holder);
			if (error_holder != 0) {
				pthread_mutex_lock(&mutex_SAFA);
				serializarYEnviarEntero(SAFA_fd,&error_holder);
				pthread_mutex_unlock(&mutex_SAFA);
				return (void*)-1;
			}

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

		char* archivo;
		log_info(log_DAM,"Recibiendo parametros para el flush");
		direccion_logica* direccion_archivo = recibirYDeserializar(socket,FLUSH_ARCHIVO);
		archivo=recibirYDeserializarString(socket);
		// Le pido al FM9 que me envíe el archivo línea a línea
		log_info(log_DAM,"Realizar Fulsh para:",archivo);

		serializarYEnviar(FM9_fd,LEER_ARCHIVO,direccion_archivo);

	    size_t message_len = 1;
	    char* buffer;
	    char *file = (char*) malloc(message_len);
	    log_info(log_DAM,"Obteniendo datos para el flush de fm9:",archivo);
		while(true) {
			// Recibo una línea
			buffer = recibirYDeserializarString(FM9_fd);
			if (string_equals_ignore_case(buffer,"FIN_ARCHIVO")) {
				break;
			} else {
				// Concateno la línea del fm9 con las líneas anteriores...
				message_len += strlen(buffer);
				file = (char*) realloc(file, message_len);
				strncat(file, buffer, message_len);
			}
		}

		// TODO: Enviar el string buffer al MDJ
		char *mandarString;
		int sizeEnviar;
		int offset=0;
		int desplazamiento_archivo;
		while (message_len!=0){
			if(message_len > config_DAM.transfer_size){
				sizeEnviar=config_DAM.transfer_size;
				message_len -= config_DAM.transfer_size;
			}
			else{
				sizeEnviar=message_len;
				message_len=0;
			}
			mandarString=(char*)malloc(sizeEnviar+1);
			desplazamiento_archivo=offset*config_DAM.transfer_size;
			memcpy(mandarString, file+desplazamiento_archivo, sizeEnviar);
			mandarString[sizeEnviar] = '\0';
			printf("Enviando: %s\n",mandarString);
			peticion_guardar guardado = {.path=archivo,.offset=offset,.size=config_DAM.transfer_size,.buffer=mandarString};
			serializarYEnviar(MDJ_fd,GUARDAR_DATOS,&guardado);
			log_info(log_DAM,"Se envio una peticion de guardado al MDJ");
			offset++;
		}
		int respuesta_safa;
		if(validarArchivoMDJ(MDJ_fd,archivo)>0){

			log_info(log_DAM,"Validacion exitosa, el archivo existe en MDJ");
			//terminacion de guardado
			peticion_guardar guardado = {.path=archivo,.offset=0,.size=0,.buffer=""};
			serializarYEnviar(MDJ_fd,GUARDAR_DATOS,&guardado);
			log_info(log_DAM,"finalizacion de guardar en MDJ");
			free(file);
			//Esto se podria poner en el switch de dam para q no espere el guardado de mdj
			int respuesta = recibirYDeserializarEntero(MDJ_fd);
			if(respuesta==GUARDAR_OK){
				log_info(log_DAM,"La operacion Flush finalizo con exito");
				respuesta_safa=FINAL_FLUSH;
			}
			else{
				log_error(log_DAM,"No hay espacio suficiente en MDJ para persistir");
				respuesta_safa=ERROR_MDJ_SIN_ESPACIO;
			}
		}
		else{
			log_error(log_DAM,"Validacion fallida, el archivo ya no existe en MDJ");
			respuesta_safa=ERROR_ARCHIVO_INEXISTENTE;
		}
		pthread_mutex_lock(&mutex_SAFA);
		serializarYEnviarEntero(SAFA_fd,&respuesta_safa);
		serializarYEnviarEntero(SAFA_fd,&direccion_archivo->pid);
		pthread_mutex_lock(&mutex_SAFA);

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

int cargarArchivoFM9(int pid, char* buffer, int* error_holder) {
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

	int header = *recibirYDeserializarEntero(FM9_fd);
	if (header == ERROR_FM9_SIN_ESPACIO) {
		log_info(log_DAM,"FM9 informó no tener suficiente espacio.");
		*error_holder = ERROR_FM9_SIN_ESPACIO;
		return -1;
	}

	log_info(log_DAM,"Enviando archivo al FM9...\n");

	direccion_logica* direccion = recibirYDeserializar(FM9_fd,ESTRUCTURAS_CARGADAS);

    char* linea = NULL;
	linea = strtok(buffer, "\n");
	cargar_en_memoria paquete = {.pid=pid,.id_segmento=direccion->base,.offset=0,.linea=NULL};

    while (linea != NULL)
    {
    	paquete.linea=linea;
    	serializarYEnviar(FM9_fd,ESCRIBIR_LINEA,&paquete);
        //printf("%i: %s\n", paquete.offset, paquete.linea);
        paquete.offset++;
        linea = strtok(NULL, "\n");
    	int header = *recibirYDeserializarEntero(FM9_fd);
    	if (header == ERROR_FM9_SIN_ESPACIO) {
    		log_info(log_DAM,"FM9 informó no tener suficiente espacio.");
    		*error_holder = ERROR_FM9_SIN_ESPACIO;
    		return -1;
    	}
    }

    return direccion->base;
}

int cargarScriptFM9(int pid, char* buffer, int* error_holder) {

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

	int header = *recibirYDeserializarEntero(FM9_fd);
	if (header == ERROR_FM9_SIN_ESPACIO) {
		log_info(log_DAM,"FM9 informó no tener suficiente espacio.");
		*error_holder = ERROR_FM9_SIN_ESPACIO;
		return -1;
	}

    char* linea = NULL;
	linea = strtok(buffer, "\n");
	cargar_en_memoria paquete = {.pid=pid,.id_segmento=0,.offset=0,.linea=NULL};

    while (linea != NULL)
    {
    	paquete.linea=linea;
    	serializarYEnviar(FM9_fd,ESCRIBIR_LINEA,&paquete);
        //printf("%i: %s\n", paquete.offset, paquete.linea);
        paquete.offset++;
        linea = strtok(NULL, "\n");
		int header = *recibirYDeserializarEntero(FM9_fd);
		if (header == ERROR_FM9_SIN_ESPACIO) {
			log_info(log_DAM,"FM9 informó no tener suficiente espacio.");
			*error_holder = ERROR_FM9_SIN_ESPACIO;
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
			log_error(log_DAM, "No recibí el header que esperaba. Esperaba %i y recibí %i", headerEsperado, headerRecibido);
			return -1;
		}
	}
	return 0;
}

void testeoFM9() {
	int error_holder;
	char bufferTesteo[200] = "crear /equipos/equipo1.txt 5\nabrir /equipos/equipo\n";
	cargarScriptFM9(0, bufferTesteo, &error_holder);
	if (error_holder != 0) {
		return;
	}

	char bufferTesteoDos[60] = "asd\ndsa\nddd\nfff\nggg\n";
	cargarArchivoFM9(0, bufferTesteoDos, &error_holder);
	if (error_holder != 0) {
		return;
	}

	char bufferTesteoTres[200] = "crear /equipos/equipo2.txt 5\nabrir /equipos/equipo2.txt jeje\notra linea\n";
	cargarScriptFM9(1, bufferTesteoTres, &error_holder);
	if (error_holder != 0) {
		return;
	}

	char bufferTesteoCuatro[60] = "\n\n\n\n\n";
	cargarArchivoFM9(1, bufferTesteoCuatro, &error_holder);
	if (error_holder != 0) {
		return;
	}
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




