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

    char *archivo;
	archivo="src/CONFIG_MDJ.cfg";

    if(validarArchivoConfig(archivo)<0)
    	return -1;

	file_MDJ=config_create(archivo);

	config_MDJ.puerto_mdj=config_get_int_value(file_MDJ,"PUERTO");
	config_MDJ.mount_point=string_duplicate(config_get_string_value(file_MDJ,"PUNTO_MONTAJE"));
	config_MDJ.time_delay=config_get_int_value(file_MDJ,"RETARDO");

	config_destroy(file_MDJ);

	//-----------------------------------------------------------------------------------------------
	//Ejecuto la consola del MDJ en un hilo aparte


	pthread_t hilo_consola;
	pthread_t hilo_conexion;

	leerMetaData();
	crearBitmap();
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

	DAM_fd=aceptarConexion(MDJ_fd);

	if(DAM_fd==-1){
		log_error(log_MDJ,"Error al establecer conexion con el DAM");
		log_destroy(log_MDJ);
		exit(0);
	}

	log_info(log_MDJ,"Conexion establecida con DAM");
	//Espero a que el DAM mande alguna solicitud y espero que me mande el tipo de operacion a realizar
	//peticion_obtener obtener = {.path="/scripts/checkpoint.escriptorio",.offset=0,.size=20};
	//crearStringDeArchivoConBloques(&obtener);
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
		int validar=VALIDAR_OK;
		if (existe_archivo(validacion->path)!=0){
			log_error(log_MDJ,"No existe el archivo:",validacion->path);
			validar=VALIDAR_FALLO;
		}
		serializarYEnviarEntero(DAM_fd, &validar);
		usleep(config_MDJ.time_delay*1000);
		break;
	}
	case CREAR_ARCHIVO:{
		peticion_crear* crear = recibirYDeserializar(fd,operacion);
		int creacion = CREAR_OK;
		log_info(log_MDJ,"peticion de creacion de archivo:",crear->path);
		if (existe_archivo(crear->path)==0){
						log_error(log_MDJ,"No se puede crear por q existe el archivo:",crear->path);
						creacion= CREAR_FALLO;
		}
		else{
			crearDirectorio(crear->path);
			crearArchivo(crear->path, crear->cant_lineas);
		}
		serializarYEnviarEntero(DAM_fd, &creacion);
		usleep(config_MDJ.time_delay*1000);
		break;
	}

	case OBTENER_DATOS:
	{
		peticion_obtener* obtener = recibirYDeserializar(fd,operacion);
		printf("Peticion de obtener..\n\tpath: %s\toffset: %d\tsize: %d\n",
							obtener->path,obtener->offset,obtener->size);
		crearStringDeArchivoConBloques(obtener);
		usleep(config_MDJ.time_delay*1000);
		break;
	}
	case GUARDAR_DATOS:
	{
		peticion_guardar* guardado = recibirYDeserializar(fd,operacion);
		printf("Peticion de guardado..\n\tpath: %s\toffset: %d\tsize: %d\tbuffer: %s\n",
				guardado->path,guardado->offset,guardado->size,guardado->buffer);

		guardarDatos(guardado);
		usleep(config_MDJ.time_delay*1000);
		break;
	}
	case BORRAR_DATOS:
	{   peticion_borrar* borrar = recibirYDeserializar(fd,operacion);
		puts(borrar->path);
		printf("Peticion de borrado..\n\tpath: %s",borrar->path);
	    borrar_archivo(borrar->path);
	    usleep(config_MDJ.time_delay*1000);
	    break;
	}

	}

}


void crearArchivo(char *path, int numero_lineas) {
    FILE* fichero_metadata;
    FILE* bloque_crear;
    //Creacion Del archivo

    char *complete_path =archivo_path(path);
    fichero_metadata = fopen(complete_path, "wr");
    fclose(fichero_metadata);

    //Carga de MetaData del archivo
	t_config *archivo_MetaData;
	archivo_MetaData=config_create(complete_path);
	char *ultimoBloqueChar;
	char *sizeDelStringArchivoAGuardarChar;
	int numeroDeBloque=config_MetaData.cantidad_bloques;
	ultimoBloqueChar=string_itoa(numeroDeBloque);
	sizeDelStringArchivoAGuardarChar=string_itoa(numero_lineas);
	char *actualizarBloques=malloc(strlen("[")+1);
	strcpy(actualizarBloques,"[");
	int i=0;
	int cantNFaltantes=numero_lineas;
	int cantNescribir;
	int flag = 1;
	while(flag !=0){
		config_MetaData.cantidad_bloques++;
		ultimoBloqueChar=string_itoa(config_MetaData.cantidad_bloques);
		string_append(&actualizarBloques,ultimoBloqueChar);
		if (cantNFaltantes < (i+1)*config_MetaData.tamanio_bloques){
			cantNescribir = cantNFaltantes;
			flag =0;
		}
		else{
			cantNFaltantes -= config_MetaData.tamanio_bloques;
			cantNescribir=config_MetaData.tamanio_bloques;
			string_append(&actualizarBloques,",");

		}
		char *pathBloqueCompleto;
		pathBloqueCompleto = bloque_path(ultimoBloqueChar);
		fichero_metadata = fopen(pathBloqueCompleto, "wr");
		for (int i=0;i<cantNescribir;i++){
		   	fprintf(fichero_metadata,"\n");
		}
		fclose(fichero_metadata);
		free(pathBloqueCompleto);
	}
    string_append(&actualizarBloques,"]");
	config_set_value(archivo_MetaData, "TAMANIO",sizeDelStringArchivoAGuardarChar);
	config_set_value(archivo_MetaData, "BLOQUES",actualizarBloques);
	config_save(archivo_MetaData);
	config_destroy(archivo_MetaData);
    actualizar_configuracion_Metadata(config_MetaData.cantidad_bloques);
    free(complete_path);
}

char *string_config_metadata(){
	char *direccionArchivoMedata=(char *) malloc(1 + strlen(config_MDJ.mount_point) + strlen("/Metadata/Metadata.bin"));;
	strcpy(direccionArchivoMedata,config_MDJ.mount_point);
	string_append(&direccionArchivoMedata,"/Metadata/Metadata.bin");
	return direccionArchivoMedata;
}

void actualizar_configuracion_Metadata(int ultimoBloque){
	t_config *archivo_MetaData;
	char *direccionArchivoMedata = string_config_metadata();
	archivo_MetaData=config_create(direccionArchivoMedata);
	char *stringUltimoBloque=string_itoa(ultimoBloque);
	config_set_value(archivo_MetaData,"CANTIDAD_BLOQUES",stringUltimoBloque);
	config_save(archivo_MetaData);
	free(direccionArchivoMedata);
	config_destroy(archivo_MetaData);
}

int guardarDatos(peticion_guardar *guardado) {

    char * complete_path = (char *) malloc(1 + strlen(config_MDJ.mount_point) + strlen("/Archivos/") + strlen(guardado->path) );
    strcpy(complete_path, config_MDJ.mount_point);
    strcat(complete_path, "/Archivos/");
    strcat(complete_path, guardado->path);


    if (!archivo_a_guardar.ocupado_archivo_a_guardar){
    	string_archivo(complete_path,&archivo_a_guardar.strig_archivo);
    	archivo_a_guardar.path=complete_path;
    	archivo_a_guardar.ocupado_archivo_a_guardar=TRUE;
    }
    else{
    	  if(guardado->size==0){
    	    	guardarEnArchivo();
    	    	return 0;
    	   }
    	  strncpy(archivo_a_guardar.strig_archivo+guardado->offset*guardado->size,guardado->buffer,guardado->size);

    }
    free(guardado);
    return 0;
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
	strcat(dir_actual,"$");
	while(1) {
		linea = readline(dir_actual);
		if(!strncmp(linea, "cerrar", 6)) {
			cerrarMDJ =1;
			break;
		}
		if(!strncmp(linea, "cd ", 3)){
			cmd_cd(linea);
		}
		if(!strncmp(linea, "pwd ", 3)){
			cmd_pwd(linea);
			printf("esta en el directorio:%s\n",dir_actual);
			strcat(dir_actual,"$");
		}
		if (!strncmp(linea, "cat ", 3)){
			cmd_cat(linea);
		}
		if (!strncmp(linea, "ls ", 2)){
			cmd_ls(linea);
		}
		if (!strncmp(linea, "bloque", 6)){
			char * path = "/equipo/test";
			//crearDirectorio(path);
			//crearArchivo(path,10000);
			//borrar_archivo(path);
			int creacion = CREAR_OK;
			log_info(log_MDJ,"peticion de creacion de archivo:",path);
			if (existe_archivo(path)!=0){
				log_error(log_MDJ,"No se puede crear por q existe el archivo:",path);
				creacion= CREAR_FALLO;
			}
			else{

				crearDirectorio(path);
				crearArchivo(path, 52);
				log_info(log_MDJ,"Se creo el archivo:",path);
			}

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
		cmd_pwd();
		strcat(dir_actual,"$");
		parametros[1]=dir_actual;
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
	printf("%d\tsize",obtener->size);
	char *archivoPathCompleto = archivo_path(obtener->path);
	t_config *archivo_MetaData;
	archivo_MetaData=config_create(archivoPathCompleto);
	t_config_MetaArchivo metadataArchivo;
	metadataArchivo.tamanio=config_get_int_value(archivo_MetaData,"TAMANIO");
	metadataArchivo.bloques=config_get_array_value(archivo_MetaData,"BLOQUES");
	int cantidadBloques=sizeof(metadataArchivo.bloques)+1;
	char * contenidoArchivo = (char *) malloc(metadataArchivo.tamanio+1);
	int sizeArchivoBloque;
	struct stat statbuf;
	int i;
	char *src;
	char *pathBloqueCompleto;
	for(i=0;i<cantidadBloques;i++){
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
		free(pathBloqueCompleto);
		close(f);

	}
	char *sub;
	int desplazamiento_archivo;
	desplazamiento_archivo=obtener->offset*obtener->size;
	int hasta=desplazamiento_archivo + obtener->size;
	if ( hasta < metadataArchivo.tamanio ){
		sub=(char*)malloc(obtener->size+1);
		memcpy(sub, contenidoArchivo+desplazamiento_archivo, obtener->size);
		sub[obtener->size] = '\0';
		printf("Enviando: %s\n",sub);
		serializarYEnviarString(DAM_fd, sub);
		free(sub);
	}
	else{
		int copiarHasta = hasta;
		copiarHasta = hasta-metadataArchivo.tamanio ;
		char *sub3 =substring(contenidoArchivo,desplazamiento_archivo, copiarHasta+1);
		printf("Enviando: %s\n",sub3);
		serializarYEnviarString(DAM_fd, sub3);
		usleep(config_MDJ.time_delay*1000);
		free(sub3);
	}

	free(contenidoArchivo);

}
char *archivo_path(char *path_archivo){

	char * complete_path = (char *) malloc(1 + strlen(config_MDJ.mount_point) + strlen("/Archivos/") + strlen(path_archivo));
    strcpy(complete_path, config_MDJ.mount_point);
    strcat(complete_path, "/Archivos/");
    strcat(complete_path, path_archivo);

    return complete_path;
}

char *bloque_path(char *numeroBloque){

	char * complete_path = (char *) malloc(1 + strlen(config_MDJ.mount_point) + strlen("/Bloques/") + strlen(numeroBloque)+ strlen(".bin"));
	strcpy(complete_path, config_MDJ.mount_point);
	strcat(complete_path, "/Bloques/");
	strcat(complete_path, numeroBloque);
	strcat(complete_path, ".bin");
	//puts(complete_path);
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
        complete_path = archivo_path(path_archivo);
        if(existe_archivo(complete_path)==0){

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

int todos_bloques_libre(char *path_archivo){
	char *path_completo = archivo_path(path_archivo);
	t_config *archivo_MetaData=config_create(path_completo);
	t_config_MetaArchivo metadataArchivo;
	metadataArchivo.tamanio=config_get_int_value(archivo_MetaData,"TAMANIO");
	metadataArchivo.bloques=config_get_array_value(archivo_MetaData,"BLOQUES");
	int cantidadBloques=sizeof(metadataArchivo.bloques)+1;
	int i=0;
	while(i<= cantidadBloques){
		char * pathBloqueCompleto;
		pathBloqueCompleto = bloque_path(metadataArchivo.bloques[i]);
		if (existe_archivo(pathBloqueCompleto)!=0) {
			return -1;
		}
		int numeroBloque;
		sscanf(metadataArchivo.bloques[i], "%d", &numeroBloque);
		printf("\nThe value of x : %d", numeroBloque);
		if (bitarray_test_bit(bitarray, numeroBloque)==1){
			printf("bloque ocupado: %d",numeroBloque);
			return -1;
		}
		else {
			printf("bloque libre: %d",numeroBloque);
		}

	}
	return 0;

}



void guardarEnArchivo(){
	//archivo_a_guardar;
	int seCreoBloque=FALSE;
	t_config *archivo_MetaData;
	archivo_MetaData=config_create(archivo_a_guardar.path);
	t_config_MetaArchivo metadataArchivo;
	metadataArchivo.tamanio=config_get_int_value(archivo_MetaData,"TAMANIO");
	metadataArchivo.bloques=config_get_array_value(archivo_MetaData,"BLOQUES");
	int cantidadBloques=sizeof(metadataArchivo.bloques)+1;
	int sizeDelStringArchivoAGuardar;
	sizeDelStringArchivoAGuardar=strlen(archivo_a_guardar.strig_archivo);
	int sizeGuardar;
	if (config_MetaData.tamanio_bloques*cantidadBloques<sizeDelStringArchivoAGuardar){
		config_MetaData.cantidad_bloques++;
		actualizarARchivo(&metadataArchivo,sizeDelStringArchivoAGuardar,config_MetaData.cantidad_bloques);
		seCreoBloque=TRUE;
	}

	for(int i=0;i<cantidadBloques;i++){
		char *pathBloqueCompleto;
		pathBloqueCompleto = bloque_path(metadataArchivo.bloques[i]);
	    char *map;
	    int size;
	    struct stat s;
	    int fd = open (pathBloqueCompleto, O_RDWR);
	    /* Get the size of the file. */
	    int status = fstat (fd, &s);
	    size = s.st_size;
	    if(status<0){
	    	printf("archivo no existe %s\n",pathBloqueCompleto);
	    }
	    map = (char *) mmap (0, size, PROT_READ, MAP_PRIVATE | PROT_WRITE, fd, 0);
	    int offset=0;
	    offset=i*config_MetaData.tamanio_bloques;
	    sizeGuardar=config_MetaData.tamanio_bloques;
	    if((i+1)*config_MetaData.tamanio_bloques<sizeDelStringArchivoAGuardar){
	    	sizeGuardar = (i+1)*config_MetaData.tamanio_bloques - sizeDelStringArchivoAGuardar;

	    }
	    memcpy(map, archivo_a_guardar.strig_archivo+offset, sizeGuardar);
	    msync(map, sizeGuardar, MS_SYNC);
	    munmap(map, sizeGuardar);
	    close(fd);
	}

}


int actualizarARchivo(t_config_MetaArchivo *metadataArchivo,int sizeDelStringArchivoAGuardar,int ultimoBloque){
	t_config *archivo_MetaData;
	archivo_MetaData=config_create(archivo_a_guardar.path);
	t_config_MetaArchivo actualizarMetadataARchivo;
	actualizarMetadataARchivo.tamanio=config_get_int_value(archivo_MetaData,"TAMANIO");
	actualizarMetadataARchivo.bloques=config_get_array_value(archivo_MetaData,"BLOQUES");
	char *ultimoBloqueChar;
	char *sizeDelStringArchivoAGuardarChar;
	//sprintf(ultimoBloqueChar, "%d", ultimoBloque);
	ultimoBloqueChar=string_itoa(ultimoBloque);
	sizeDelStringArchivoAGuardarChar=string_itoa(sizeDelStringArchivoAGuardar);
	config_set_value(archivo_MetaData, "TAMANIO",sizeDelStringArchivoAGuardarChar);
	char *actualizarBloques=malloc(strlen("[")+1);
	strcpy(actualizarBloques,"[");
    int cantidadDebloques=sizeof(actualizarMetadataARchivo.bloques)+1;
	for(int i=0;i<cantidadDebloques; i++){
		puts(actualizarMetadataARchivo.bloques[i]);
		string_append(&actualizarBloques,actualizarMetadataARchivo.bloques[i]);
		string_append(&actualizarBloques,",");
	}

	string_append(&actualizarBloques,ultimoBloqueChar);
	string_append(&actualizarBloques,"]");
	config_set_value(archivo_MetaData, "BLOQUES",actualizarBloques);
	config_save(archivo_MetaData);
	metadataArchivo->bloques=config_get_array_value(archivo_MetaData,"BLOQUES");
	return 0;
}

char *substring(char *string, int position, int length)
{
   char *pointer;
   int c;

   pointer = malloc(length+1);

   if (pointer == NULL)
   {
      printf("Unable to allocate memory.\n");
      exit(1);
   }

   for (c = 0 ; c < length ; c++)
   {
      *(pointer+c) = *(string+position-1);
      string++;
   }

   *(pointer+c) = '\0';

   return pointer;
}


void crearBitmap(){

	//FILE* bloque_crear;
	//bloque_crear->_IO_buf_base
	char *direccionArchivoBitMap=(char *) malloc(1 + strlen(config_MDJ.mount_point) + strlen("/Metadata/Bitmap.bin"));;
	int bitmap = open(direccionArchivoBitMap, O_RDWR);
	struct stat mystat;
	if (fstat(bitmap, &mystat) < 0) {
	    printf("Error al establecer fstat\n");
	    close(bitmap);
	}
    char *bmap ;
	bmap = mmap(NULL, mystat.st_size, PROT_WRITE | PROT_READ, MAP_SHARED, bitmap, 0);

	if (bmap == MAP_FAILED) {
			printf("Error al mapear a memoria: %s\n", strerror(errno));

	}

	bitarray = bitarray_create_with_mode(bmap, config_MetaData.cantidad_bloques/8, MSB_FIRST);

}
void crearDirectorio(char *path){
	int i;
	char **arr = NULL;
	arr = string_split(path, "/");
	int c=sizeof(arr)+1;
	printf("cantidad de parametros:%d\n", c);
    if (c==1){
    	printf( "no hay directorio para crear %s\n",arr[0] );
    }
    else{
    	for (i = 0; i < c; i++){
    	    	struct stat info;
    	    	if( stat( arr[i], &info ) != 0 ){
    	    		char* full_directorio= archivo_path(arr[i]);
    	    	    if (arr[i+1]==NULL){
    	    	    	break;
    	    	    }
    	    	    printf( "el directorio no esta creado %s\n",arr[i] );
    	    		if (mkdir(full_directorio, S_IRWXU) != 0) {
    	    	        if (errno != EEXIST)
    	    	        	printf( "no se pudo crear el directorio %s\n",full_directorio);
    	    	    }
    	    	}
    	    	else if( info.st_mode & S_IFDIR )
    	    	    printf( "%s is a directory\n", arr[i] );
    	    	else
    	    	    printf( "%s es un archivo \n", arr[i] );
   	    }

    }



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


