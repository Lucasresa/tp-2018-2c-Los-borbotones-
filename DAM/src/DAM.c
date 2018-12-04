#include "DAM.h"

int main(){

	log_DAM = log_create("DAM.log","DAM",true,LOG_LEVEL_INFO);

    char *archivo;
	archivo="src/CONFIG_DAM.cfg";
    if(validarArchivoConfig(archivo) <0)
    	return -1;

	file_DAM = config_create(archivo);

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
	int length;
	int header;
	length = recv(socket,&header,sizeof(int),0);
	if ( length <= 0 ) {
		close(socket);
		FD_CLR(socket, &set_fd);
		log_error(log_DAM, "Se perdió la conexión con un socket.");
		perror("error");
		return 0;
	}

	switch(header){
	case DESBLOQUEAR_DUMMY:
	{
		desbloqueo_dummy* dummy;
		dummy = recibirYDeserializar(socket,header);
		log_info(log_DAM,"Recibi el header desbloquear dummy %s", dummy->path);
		char* buffer = obtenerArchivoMDJ(dummy->path);

		char bufferTesteo[60] = "crear /equipos/equipo1.txt 5\nabrir /equipos/equipo\n";
		log_info(log_DAM,"Cargo archivo al FM9");
		cargarArchivoFM9(dummy->id_dtb, buffer);
		log_info(log_DAM,"Enviando final carga dummy");

		int success=FINAL_CARGA_DUMMY;
		serializarYEnviarEntero(SAFA_fd,&success);
	    serializarYEnviarEntero(SAFA_fd,&dummy->id_dtb);
	    log_info(log_DAM,"Final carga dummy enviado al safa");
		return 0;
	}
	case CREAR_ARCHIVO:
	{
		log_info(log_DAM,"Peticion de crear archivo recibida");
		peticion_crear* crear_archivo=recibirYDeserializar(socket,header);
		int dtb_id = *recibirYDeserializarEntero(socket);

		log_info(log_DAM,"Enviando peticion de crear archivo a MDJ");
		serializarYEnviar(MDJ_fd,header,crear_archivo);
		int respuesta;

		respuesta = *recibirYDeserializarEntero(MDJ_fd);
		printf("Recibi: %d\n",respuesta);

		switch(respuesta){

			case CREAR_OK:{

				printf("creacion ok\n");
				int success=FINAL_CREAR;
				serializarYEnviarEntero(SAFA_fd,&success);
				serializarYEnviarEntero(SAFA_fd,&dtb_id);
				log_info(log_DAM,"El archivo se creo con exito");
				return 0;
			}
			case CREAR_FALLO:{
				printf("creacion fallida\n");
			    int success=ERROR_ARCHIVO_EXISTENTE;
				serializarYEnviarEntero(SAFA_fd,&success);
				serializarYEnviarEntero(SAFA_fd,&dtb_id);
				log_info(log_DAM,"Hubo un error al crear el archivo");
				return 0;
			}
			default:
				printf("fallo el enum crear");
		}
	}
	case ABRIR_ARCHIVO:
	{
		log_info(log_DAM,"Peticion de abrir archivo recibida");

		char*path = recibirYDeserializarString(socket);
		int dtb_id = *recibirYDeserializarEntero(socket);

		char* buffer = obtenerArchivoMDJ(path);

		log_info(log_DAM,"Cargo archivo al FM9");
		cargarArchivoFM9(dtb_id, buffer);
		log_info(log_DAM,"Enviando Final abrir archivo");

		int success=FINAL_ABRIR;
		serializarYEnviarEntero(SAFA_fd,&success);
	    serializarYEnviarEntero(SAFA_fd,&dtb_id);
	    serializarYEnviarString(SAFA_fd,path);
	    //Aca hay que agregar el "acceso" para que el dtb sepa acceder al archivo
	    log_info(log_DAM,"Final carga dummy enviado al safa");
		return 0;
	}
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
