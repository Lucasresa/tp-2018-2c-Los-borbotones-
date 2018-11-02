/*
 ============================================================================
 Name        : MDJ.c
 Author      : german jugo
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <commons/temporal.h>
#include <commons/config.h>
#include <netdb.h> // Para getaddrinfo
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <openssl/md5.h>
#include <sys/wait.h>
#include <commons/config.h>
#include <commons/collections/list.h>
#include <pthread.h>
#include "MDJ.h"
#include <errno.h>
#include <dirent.h>



void consola(){
	char * linea;
	dir_actual="/";
	printf("\n");
	chdir(dir_actual);
	//char *string_actual;

	//string_append(&dir_actual,"$");
	cmd_pwd();
	while(1) {
		linea = readline(dir_actual);
		if(!strncmp(linea, "cerrar", 6)) {
			cerrarMDJ =1;
			break;
		}
		if(!strncmp(linea, "cd ", 3)){
			cmd_cd(linea);
		}
		if (!strncmp(linea, "cat ", 3)){
			cmd_cat(linea);
		}
		if (!strncmp(linea, "ls ", 2)){
			cmd_ls(linea);
		}
		if (!strncmp(linea, "bloque", 6)){
					cmd_bloque(linea);
		}


	}
	free(linea);
}

void configure_logger() {
   logger = log_create("OperacionesMDJ.log", "OperacionesMDJ", true, LOG_LEVEL_INFO);
}
void exit_gracefully(int return_nr) {
   log_destroy(logger);
  exit(return_nr);
}

void leerArchivoConfiguracion(t_config * archivoConfig){
	log_MDJ = log_create("MDF.log","MDJ",true,LOG_LEVEL_INFO);
	//Levanto archivo de configuracion del MDJ (FileSystem)
	file_MDJ=config_create("CONFIG_MDJ.cfg");
	config_MDJ.puerto_mdj=config_get_int_value(archivoConfig,"PUERTO");
	config_MDJ.mount_point=string_duplicate(config_get_string_value(archivoConfig,"PUNTO_MONTAJE"));
	config_MDJ.time_delay=config_get_int_value(archivoConfig,"RETARDO");
	config_destroy(archivoConfig);


}

void cmd_cat(char *linea){
	char **parametros = string_split(linea, " ");
	if(parametros[1] == NULL){
		printf("El Comando cat debe recibir un parametro.\n");
	}
	else{
		char *content;
		if ((string_archivo(parametros[1],&content))>0){
			puts(content);
		}
	}
}
void cmd_cd(char *linea){
	char **parametros = string_split(linea, " ");
		if(parametros[1] == NULL){
				printf("El Comando cd debe recibir un parametro.\n");
		}
		else{
			printf("Directorio: %s\n", parametros[1]);
			if (chdir(parametros[1])) {
				perror(parametros[1]);
				//dir_actual = parametros[1];
			}
			else{
				chdir(parametros[1]);
				cmd_pwd();
			}
	 }
}
void cmd_ls(char *linea){
	char **parametros = string_split(linea, " ");
	puts(parametros[1]);
	if(parametros[1] == NULL){
		printf("El Comando cat debe recibir un parametro.\n");
	}
	else {
		 DIR *dp;
		 struct dirent *ep;
		 dp = opendir (parametros[1]);
		 if (dp != NULL){
		      while ((ep = readdir (dp))!= NULL){
			        printf("%s\n",ep->d_name);
		      }
		      (void) closedir (dp);
		 }
		 else{
			  perror ("Couldn't open the directory");
		 }

	}
}
void cmd_pwd(){
	char cwd[PATH_MAX];
	if (getcwd(cwd, sizeof(cwd)) != NULL) {
	       dir_actual=cwd;
	}
	else
	{
	       perror("getcwd() error");
	}
	string_actual=dir_actual;
	strcat(string_actual,"$");
}
void cmd_bloque(){
	char *file1;
	char *file2;
	file1 = "1.txt";
	file2 = "2.txt";
	char *contenido;
	string_archivo(file1,&contenido);
	char *agregar_contenido;
	string_archivo(file2,&agregar_contenido);
	//strcat(contenido,agregar_contenido);
	string_append(&contenido,agregar_contenido);
	printf("%s",contenido);
	//puts(contenido);
	//puts("fermamdp");
}

int string_archivo(char *pathfile,char **contenido_archivo){
	char *contentenido;
	//printf("Parametros %s.\n", pathfile);
	int size = 0;
	FILE *f = fopen(pathfile, "rb");
	if (f == NULL){
		printf("no se encontro el archivo %s.\n", pathfile);
		return -1;
	}
	fseek(f, 0, SEEK_END);
	//fseek(f, 0, SEEK_CUR-1);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);
	contentenido = (char *)malloc(size+1);
	if (size != fread(contentenido, sizeof(char), size, f))	{
		free(contentenido);
	}
	//puts(contentenido);
	fclose(f);
	*contenido_archivo = contentenido;
	return 1;
}
int iniciar_recibirDatos(){

	int tipo_operacion,bytesRecibidos;

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
		if(existe_archivo(validacion->path)==0){
			//send();

		}
		else
			//send();
			puts("Hola");
		break;

	}
	case CREAR_ARCHIVO:{
		peticion_crear* creacion = recibirYDeserializar(fd,operacion);
		crearArchivo(creacion->path, creacion->cant_lineas);
		break;
	}

	case OBTENER_DATOS:
	{
		peticion_obtener* obtener = recibirYDeserializar(fd,operacion);
		break;
	}
	case BORRAR_ARCHIVO:
	{
			peticion_borrar* borrar = recibirYDeserializar(fd,operacion);
			borrar_archivo(borrar->path);
			break;
	}

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
	return 0;
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
int inicializar(){

	leerMetaData();
	return 0;

}

int leerMetaData(){
	char *direccionArchivoMedata;
	direccionArchivoMedata = config_MDJ.mount_point;
	strcpy(direccionArchivoMedata,config_MDJ.mount_point);
	string_append(&direccionArchivoMedata,"/Metadata/Metadata.bin");
	t_config *archivo_MetaData;
	archivo_MetaData=config_create(direccionArchivoMedata);
	config_MetaData.cantidad_bloques=config_get_int_value(archivo_MetaData,"CANTIDAD_BLOQUES");
    config_MetaData.magic_number=string_duplicate(config_get_string_value(archivo_MetaData,"MAGIC_NUMBER"));
	config_MetaData.tamanio_bloques=config_get_int_value(archivo_MetaData,"TAMANIO_BLOQUES");
	config_destroy(archivo_MetaData);
	return 0;
}

int  conexion_dam(){
	int MDJ_fd;
	crearSocket(&MDJ_fd);
	setearParaEscuchar(&MDJ_fd, config_MDJ.puerto_mdj);
	log_info(log_MDJ, "Esperando conexion del DAM");

	DAM_fd=aceptarConexion(MDJ_fd);
	if(DAM_fd==-1){
			log_error(log_MDJ,"Error al establecer conexion con el DAM");
			log_destroy(log_MDJ);
			return -1;
	}
	log_info(log_MDJ,"Conexion establecida con DAM");
	iniciar_recibirDatos();
	return 0;
}

char *archivo_bit(char *path_archivo){

	char * complete_path = (char *) malloc(1 + strlen(config_MDJ.mount_point) + strlen("/Archivos/") + strlen(path_archivo) + strlen(".bit") );
    strcpy(complete_path, config_MDJ.mount_point);
    strcat(complete_path, "/Archivos/");
    strcat(complete_path, path_archivo);
    strcat(complete_path, ".bit");
}

int existe_archivo(char *path_archivo){
	struct stat   buffer;
	return (stat (archivo_bit(path_archivo), &buffer) == 0);
}

int borrar_archivo(char *path_archivo){
	char * complete_path;
	if(existe_archivo(path_archivo)==0){
		complete_path = archivo_bit(path_archivo);
		remove(complete_path);
		return 0;
	}
	else
		return -1;

}
/*int cmd_md5(char *linea){
	char **parametros = string_split(linea, " ");
	puts(parametros[1]);
	if(parametros[1] == NULL){
			printf("El Comando md5 debe recibir un parametro.\n");
			return -1;
	}
	char *contenido;
	int size;
	string_file(parametros[1] ,&contenido);
	MD5_CTX md5;
	size=sizeof(contenido);
	void * md5_resultado = malloc(size+1);
	MD5_Init(&md5);
	MD5_Update(&md5,contenido , size+1);
	MD5_Final(md5_resultado, &md5);
	free(contenido);
	puts(md5_resultado);
	return 0;
}
*/
