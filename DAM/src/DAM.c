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

	if(conectar(&MDJ_fd,config_DAM.puerto_mdj,config_DAM.ip_mdj)!=0){
		log_error(log_DAM,"Error al conectarse con MDJ");
		exit(1);
	}
	else{
		log_info(log_DAM,"Conexion con MDJ establecida");
	}

	while(true) {
		escuchar(listener_socket, &set_fd, &funcionHandshake, NULL, &recibirPeticion, NULL );
	}
/*
	//El DAM se conecta con FM9_fd
	if(conectar(&FM9_fd,config_DAM.puerto_fm9,config_DAM.ip_fm9)!=0){
		log_error(log_DAM,"Error al conectarse con FM9");
		exit(1);
	} else {
		log_info(log_DAM, "Conexión con FM9 establecido");
	}
*/
	//El DAM se conecta con MDJ


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
		log_info(log_DAM,"Recibi el header desbloquear dummy");
		desbloqueo_dummy* dummy;
		dummy = recibirYDeserializar(socket,header);
		log_info(log_DAM,"Recibi el header desbloquear dummy %s", dummy->path);
		char* misVeinteBytes = obtenerArchivoMDJ(dummy->path);
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
	peticion_obtener obtener = {.path="/scripts/checkpoint.escriptorio",.offset=0,.size=20};

	log_info(log_DAM,"Enviando peticion al MDJ...");

	serializarYEnviar(MDJ_fd,OBTENER_DATOS,&obtener);
	log_info(log_DAM,"Peticion envada...");
	char *buffer = malloc(sizeof(char) * 21);
	log_info(log_DAM,"asd");
	while(1) {
		buffer = recibirYDeserializarString(MDJ_fd);
		//printf("Recibi: %s\n",buffer);
		break;
	}
	log_info(log_DAM,"asd");

	return buffer;
}

int cargarArchivoFM9(int pid, char* buffer) {

	//int contador_offset;
	iniciar_scriptorio_memoria* datos_script = malloc(sizeof(iniciar_scriptorio_memoria));

	datos_script->pid = pid;
	datos_script->size_script = (int)strlen(buffer);
	puts("enviando datos_script");
	serializarYEnviar(FM9_fd,INICIAR_MEMORIA_PID,datos_script);
	free(datos_script);
	//serializarYEnviarEntero(FM9_fd, &tamanio_buffer);

	int header;
	int length = recv(FM9_fd,&header,sizeof(int),0);
	if ( length == -1 ) {
		perror("error");
		log_error(log_DAM, "Socket FM9 sin conexión.");
		close(FM9_fd);
		return -1;
	}

	if (header != MEMORIA_INICIALIZADA) {
		length = recv(FM9_fd,&header,sizeof(int),0);
		if (header != MEMORIA_INICIALIZADA) {
			log_error(log_DAM, "No recibí el header que esperaba del FM9.");
		}
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
    }
    return 1;
}
