/*
 * MDJ.c
 *
 *  Created on: 2 sep. 2018
 *      Author: utnso
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
#include <errno.h>
#include <dirent.h>
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
	pthread_t hilo_conexion;

	leerMetaData();
	cerrarMDJ=0;
	pthread_create(&hilo_consola,NULL,(void*)consola_MDJ,NULL);
	pthread_create(&hilo_conexion,NULL,(void*)conexion_DMA,NULL);
	//conexion_DMA();

	//if (crear_carpetas() != 0) {
	//	log_error(log_MDJ,"Error al crear los directorios para el MDJ");
	//	return -1;
	//}

	//log_info(log_MDJ,"Directorios creados");

	//-----------------------------------------------------------------------------------------------
    //Espero que el DAM se conecte y logueo cuando esto ocurra

	cerrado_cosola=0;
	cerrado_conexion=0;
	while(1){
		if(cerrarMDJ==1)
			break;
	}
	pthread_detach(hilo_consola);
	pthread_detach(hilo_conexion);

}
void conexion_DMA(){
	log_info(log_MDJ,"Iniciar conexion a DMA");
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
		if(cerrarMDJ==1)
			break;

		bytesRecibidos=recv(DAM_fd,&tipo_operacion,sizeof(int),0);

		if(bytesRecibidos<=0){

			log_error(log_MDJ,"Error al recibir la operacion del DAM");
			log_destroy(log_MDJ);
			exit(1);
		}
		cerrado_conexion=1;
		log_info(log_MDJ,"Peticion recibida del DAM, procesando....");

		determinarOperacion(tipo_operacion,DAM_fd);


	}
	close(MDJ_fd);

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
	{
		peticion_obtener* obtener = recibirYDeserializar(fd,operacion);
		printf("Peticion de guardado..\n\tpath: %s\toffset: %d\tsize: %d\n",
							obtener->path,obtener->offset,obtener->size);
		crearStringDeArchivoConBloques(obtener);

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
	case BORRAR_ARCHIVO:
	{   peticion_borrar* borrar = recibirYDeserializar(fd,operacion);
		puts(borrar->path);
		printf("Peticion de borrado..\n\tpath: %s",borrar->path);
	    borrar_archivo(borrar->path);
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

void consola_MDJ(){
	log_info(log_MDJ,"Consola en linea");
	char * linea;
	dir_actual="/";
	printf("\n");
	chdir(dir_actual);


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
	free(dir_actual);
	free(linea);
	free(string_actual);
	cerrado_cosola=1;
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
			free(content);
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
	string_append(&contenido,agregar_contenido);
	//strcat(contenido,agregar_contenido);
	printf("%s",contenido);
	//puts(contenido);
	//puts("fermamdp");
	//free(file1);
	//free(file2);
	free(contenido);
}

int string_archivo(char *pathfile,char **contenido_archivo){
	char *contentenido;
	int size = 0;
	FILE *f = fopen(pathfile, "rb");
	if (f == NULL){
		printf("no se encontro el archivo %s.\n", pathfile);
		return -1;
	}
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);
	contentenido = (char *)malloc(size+1);
	if (size != fread(contentenido, sizeof(char), size, f))	{
		free(contentenido);
	}
	fclose(f);
	*contenido_archivo = contentenido;
	return 1;
}

void crearStringDeArchivoConBloques(peticion_obtener *obtener){
	char *archivoPathCompleto = archivo_path(obtener->path);
	puts(archivoPathCompleto);
	char *path;
	puts(archivoPathCompleto);
	t_config *archivo_MetaData;
	archivo_MetaData=config_create(archivoPathCompleto);
	t_config_MetaArchivo metadataArchivo;
	metadataArchivo.tamanio=config_get_int_value(archivo_MetaData,"TAMANIO");
	metadataArchivo.bloques=config_get_array_value(archivo_MetaData,"BLOQUES");
	int cantidadBloques=sizeof(metadataArchivo.bloques)+1;
	char * contenidoArchivo = (char *) malloc(metadataArchivo.tamanio+1);
	int sizeArchivoBloque;
	int size = 0;
	struct stat statbuf;
	int i;
	char *src;
	char *pathBloqueCompleto;
	for(i=0;i<cantidadBloques;i++){
		metadataArchivo.tamanio=metadataArchivo.tamanio -sizeArchivoBloque;
		if(metadataArchivo.tamanio >=config_MetaData.tamanio_bloques){
			sizeArchivoBloque = config_MetaData.tamanio_bloques;
		}
		else{
			sizeArchivoBloque = metadataArchivo.tamanio;
		}
		pathBloqueCompleto = bloque_path(metadataArchivo.bloques[i]);

		int f = open(pathBloqueCompleto, O_RDONLY);
		if (f < 0){
			printf("no se encontro el archivo %s.\n",pathBloqueCompleto );
		}

		if(i==metadataArchivo.tamanio){

			if (fstat (f,&statbuf) < 0){
				printf ("fstat error");
			}
			sizeArchivoBloque=statbuf.st_size - 1;
		}


		if ((src = mmap (0, sizeArchivoBloque, PROT_READ, MAP_SHARED, f, 0))== (caddr_t) -1)
		{
			printf ("mmap error for input");

		}

		if(i==0){
			strcpy(contenidoArchivo,src);
		}
		else
			string_append(&contenidoArchivo,src);
		puts(contenidoArchivo);
		free(pathBloqueCompleto);
		free(contenidoArchivo);
		close(f);
		//string_archivo(file2,&agregar_contenido);
		//string_append(&contenido,agregar_contenido);

		//string_archivo(pathBloqueCompleto,&contenido);
		//strcat(src,contenido);
		//printf("%s",src);
		//puts(contenido);

	}
	char *sub;
	int desplazamiento_archivo;
	desplazamiento_archivo=obtener->offset*obtener->size +1;
	strncpy(sub, contenidoArchivo+desplazamiento_archivo, obtener->size);
	sub[obtener->size] = '\0';
	//puts(contenidoArchivo);
	//config_destroy(archivo_MetaData);
	//puts(contenido);
	//return src;
	//return contenidoArchivo;
	free(sub);

}
char *archivo_path(char *path_archivo){

	char * complete_path = (char *) malloc(1 + strlen(config_MDJ.mount_point) + strlen("/Archivos/") + strlen(path_archivo));
    strcpy(complete_path, config_MDJ.mount_point);
    strcat(complete_path, "/Archivos/");
    strcat(complete_path, path_archivo);
    puts(path_archivo);
    return complete_path;
}
char *bloque_path(char *numeroBloque){

	char * complete_path = (char *) malloc(1 + strlen(config_MDJ.mount_point) + strlen("/Bloques/") + strlen(numeroBloque)+ strlen(".bin"));
	strcpy(complete_path, config_MDJ.mount_point);
	strcat(complete_path, "/Bloques/");
	strcat(complete_path, numeroBloque);
	strcat(complete_path, ".bin");
	puts(complete_path);
	return complete_path;
}
int leerMetaData(){
	char *direccionArchivoMedata=(char *) malloc(1 + strlen(config_MDJ.mount_point) + strlen("/Metadata/Metadata.bin"));;
	//direccionArchivoMedata = config_MDJ.mount_point;
	strcpy(direccionArchivoMedata,config_MDJ.mount_point);
	string_append(&direccionArchivoMedata,"/Metadata/Metadata.bin");
	t_config *archivo_MetaData;
	archivo_MetaData=config_create(direccionArchivoMedata);
	config_MetaData.cantidad_bloques=config_get_int_value(archivo_MetaData,"CANTIDAD_BLOQUES");
    config_MetaData.magic_number=string_duplicate(config_get_string_value(archivo_MetaData,"MAGIC_NUMBER"));
	config_MetaData.tamanio_bloques=config_get_int_value(archivo_MetaData,"TAMANIO_BLOQUES");
	free(direccionArchivoMedata);
	config_destroy(archivo_MetaData);
	return 0;
}

int borrar_archivo(char *path_archivo){
        char * complete_path;
        if(existe_archivo(path_archivo)==0){
        	    complete_path = archivo_path(path_archivo);
                remove(complete_path);
                free(complete_path);
                return 0;
        }
        else
                return -1;

}

int existe_archivo(char *path_archivo){
        struct stat   buffer;
        return (stat (archivo_path(path_archivo), &buffer) == 0);
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
