/*
 * MDJ.c
 *
 *  Created on: 2 sep. 2018
 *      Author: utnso
 */

#include "MDJ.h"

int main(){
	//Creo un Log para MDJ (FileSystem)
    log_MDJ = log_create("MDF.log","MDJ",true,LOG_LEVEL_INFO);

	//Levanto archivo de configuracion del MDJ (FileSystem)
	file_MDJ=config_create("CONFIG_MDJ.cfg");

	config_MDJ.puerto_mdj=config_get_int_value(file_MDJ,"PUERTO");
	config_MDJ.mount_point=string_duplicate(config_get_string_value(file_MDJ,"PUNTO_MONTAJE"));
	config_MDJ.time_delay=config_get_int_value(file_MDJ,"RETARDO");

	config_destroy(file_MDJ);

	//-----------------------------------------------------------------------------------------------
	//Ejecuto la consola del MDJ en un hilo aparte


	pthread_t hilo_consola;

	pthread_create(&hilo_consola,NULL,(void*)consola_MDJ,NULL);

	pthread_detach(hilo_consola);

	log_info(log_MDJ,"Consola en linea");

	if (crear_carpetas() != 0) {
		log_error(log_MDJ,"Error al crear los directorios para el MDJ");
		return -1;
	}

	log_info(log_MDJ,"Directorios creados");

	//-----------------------------------------------------------------------------------------------
    //Espero que el DAM se conecte y logueo cuando esto ocurra


	int tipo_operacion,bytesRecibidos;

	int MDJ_fd;
	crearSocket(&MDJ_fd);
	setearParaEscuchar(&MDJ_fd, config_MDJ.puerto_mdj);

	log_info(log_MDJ, "Esperando conexion del DAM");

	int DAM_fd=aceptarConexion(MDJ_fd);

	if(DAM_fd==-1){
		log_error(log_MDJ,"Error al establecer conexion con el DAM");
		log_destroy(log_MDJ);
		exit(0);
	}

	log_info(log_MDJ,"Conexion establecida con DAM");


	//Espero a que el DAM mande alguna solicitud y espero que me mande el tipo de operacion a realizar
	while(1){

		pthread_t hilo_operacion;

		bytesRecibidos=recv(DAM_fd,&tipo_operacion,sizeof(int),0);

		if(bytesRecibidos<=0){

			log_error(log_MDJ,"Error al recibir la operacion del DAM");
			log_destroy(log_MDJ);
			//exit(1);
		}

		log_info(log_MDJ,"Peticion recibida del DAM, procesando....");

		determinarOperacion(tipo_operacion,DAM_fd);

	}

}

void determinarOperacion(int operacion,int fd) {

	switch(operacion){

	case VALIDAR_ARCHIVO:
	{
		peticion_validar* validacion = recibirYDeserializar(fd,operacion);
		printf("Peticion de validar recibida con el path: %s\n",validacion->path);
		break;
	}
	case CREAR_ARCHIVO:
		crearArchivo("ejemplo.txt", 0);
		break;
	case OBTENER_DATOS:

		break;
	case GUARDAR_DATOS:
	{
		peticion_guardar* guardado = recibirYDeserializar(fd,operacion);
		printf("Peticion de guardado..\n\tpath: %s\toffset: %d\tsize: %d\tbuffer: %s\n",
				guardado->path,guardado->offset,guardado->size,guardado->buffer);

		//guardarDatos(guardado->path, 0, 0, guardado->buffer);
		break;
	}
	}

}

void crearArchivo(char *path, int numero_lineas) {
    FILE* fichero;

    char * complete_path = (char *) malloc(1 + strlen(config_MDJ.mount_point) + strlen("/Archivos/") + strlen(path) );
    strcpy(complete_path, config_MDJ.mount_point);
    strcat(complete_path, "/Archivos/");
    strcat(complete_path, path);

    fichero = fopen(complete_path, "wr");
    free(complete_path);
    fclose(fichero);

}

void guardarDatos(char *path, int size, int offset, char *buffer) {

    FILE* fichero;

    char * complete_path = (char *) malloc(1 + strlen(config_MDJ.mount_point) + strlen("/Archivos/") + strlen(path) );
    strcpy(complete_path, config_MDJ.mount_point);
    strcat(complete_path, "/Archivos/");
    strcat(complete_path, path);

    fichero = fopen(complete_path, "r+");
    fseek(fichero, 0, SEEK_END);
    fputs(buffer, fichero);

    free(complete_path);
    fclose(fichero);
}

int crear_carpetas() {

	if (mkdir_p(config_MDJ.mount_point) != 0) {
		return -1;
	}

    char * inner_folders = (char *) malloc(1 + strlen(config_MDJ.mount_point)+ strlen("/Archivos") );
    strcpy(inner_folders, config_MDJ.mount_point);
    strcat(inner_folders, "/Archivos");

	if (mkdir_p(inner_folders) != 0) {
		return -1;
	}
	free(inner_folders);
}

int mkdir_p(const char *path)
{
    /* Adapted from http://stackoverflow.com/a/2336245/119527 */
    const size_t len = strlen(path);
    char _path[256];
    char *p;

    errno = 0;

    /* Copy string so its mutable */
    if (len > sizeof(_path)-1) {
        errno = ENAMETOOLONG;
        return -1;
    }
    strcpy(_path, path);

    /* Iterate the string */
    for (p = _path + 1; *p; p++) {
        if (*p == '/') {
            /* Temporarily truncate */
            *p = '\0';

            if (mkdir(_path, S_IRWXU) != 0) {
                if (errno != EEXIST)
                    return -1;
            }

            *p = '/';
        }
    }

    if (mkdir(_path, S_IRWXU) != 0) {
        if (errno != EEXIST)
            return -1;
    }

    return 0;
}
