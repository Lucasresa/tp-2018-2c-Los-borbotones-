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

	archivo_a_guardar = malloc(sizeof(t_config_archivo_a_guardar));
	archivo_a_guardar->ocupado_archivo_a_guardar=0;

	//-----------------------------------------------------------------------------------------------
	//Ejecuto la consola del MDJ en un hilo aparte


	pthread_t hilo_consola;
	pthread_t hilo_conexion;

	char *direccionArchivoMedata=(char *) malloc(1 + strlen(config_MDJ.mount_point) + strlen("/Metadata/Metadata.bin"));;
	strcpy(direccionArchivoMedata,config_MDJ.mount_point);
	string_append(&direccionArchivoMedata,"/Metadata/Metadata.bin");
	if (validarArchivoConfig(direccionArchivoMedata)<0){
		log_error(log_MDJ,"No se encontro el file system en el punto de montaje");
		return -1;
	}
	leerMetaData();
	crearEstructura();

	crearBitmap();
	cerrarMDJ=0;
	pthread_create(&hilo_consola,NULL,(void*)consola_MDJ,NULL);
	pthread_create(&hilo_conexion,NULL,(void*)conexion_DMA,NULL);

	//-----------------------------------------------------------------------------------------------
    //Espero que el DAM se conecte y logueo cuando esto ocurra

	cerrado_cosola=0;
	cerrado_conexion=0;
	while(1){
		if(cerrarMDJ==1)
			break;
	}
	free(archivo_a_guardar);
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
		char *pathCompleto = archivo_path(validacion->path);
		if (validarArchivoConfig(pathCompleto)<0){
			log_info(log_MDJ,"No existe el archivo:",validacion->path);
			log_error(log_MDJ,"No existe el archivo:",validacion->path);
			log_error(log_MDJ,"Se va a enviar una validacion de fallo a DAM:");
			validar=VALIDAR_FALLO;
		}
		log_info(log_MDJ,"Se envia validacion a DAM::");
		usleep(config_MDJ.time_delay*1000);
		serializarYEnviarEntero(DAM_fd, &validar);
		break;
	}
	case CREAR_ARCHIVO:{
		peticion_crear* crear = recibirYDeserializar(fd,operacion);
		int creacion = CREAR_OK;
		log_info(log_MDJ,"Se recibio una peticion de creacion de archivo:");
		log_info(log_MDJ,"peticion de creacion de archivo:",crear->path);
		if (hayEspacio(crear)!=0){
								log_info(log_MDJ,"No se puede crear archivo: por q no hay espacio",crear->path);
								log_error(log_MDJ,"No se puede crear por q no hay espacio/ bloques libres:",crear->path);
								creacion= CREAR_FALLO;
		}
		else{
			log_info(log_MDJ,"Creacion de direcctorios si fuera necesario para archivo:",crear->path);
			crearDirectorio(crear->path);
			log_info(log_MDJ,"Creacion de archivo:",crear->path);
			crearArchivo(crear->path, crear->cant_lineas);
		}
		log_info(log_MDJ,"Se envia el codigo de creacion de archivo:");
		serializarYEnviarEntero(DAM_fd, &creacion);
		usleep(config_MDJ.time_delay*1000);
		break;
	}

	case OBTENER_DATOS:
	{
		peticion_obtener* obtener = recibirYDeserializar(fd,operacion);
		printf("Peticion de obtener..\n\tpath: %s\toffset: %d\tsize: %d\n",obtener->path,obtener->offset,obtener->size);
		crearStringDeArchivoConBloques(obtener);
		usleep(config_MDJ.time_delay*1000);
		break;
	}
	case GUARDAR_DATOS:
	{
		log_info(log_MDJ,"Recibido Peticion de guardado de DAM:");
		peticion_guardar* guardado = recibirYDeserializar(fd,operacion);
		log_info(log_MDJ,"Peticion de guardado de DAM para:",guardado->path);
		guardarDatos(guardado);
		usleep(config_MDJ.time_delay*1000);
		break;
	}
	case BORRAR_DATOS:
	{   peticion_borrar* borrar = recibirYDeserializar(fd,operacion);
		log_info(log_MDJ,"Peticion de Borrado de DAM para:",borrar->path);
		borrar_archivo(borrar->path);
	    log_info(log_MDJ,"Archivo borrado y bloques liberado para::",borrar->path);
	    usleep(config_MDJ.time_delay*1000);
	    break;
	}

	}

}


void crearArchivo(char *path, int numero_lineas) {
    FILE* fichero_metadata;
    //FILE* bloque_crear;
    //Creacion Del archivo

    char *complete_path =archivo_path(path);
    fichero_metadata = fopen(complete_path, "wr");
    //fichero_metadata->
    fclose(fichero_metadata);

    //Carga de MetaData del archivo
	t_config *archivo_MetaData;
	archivo_MetaData=config_create(complete_path);
	char *ultimoBloqueChar;
	char *sizeDelStringArchivoAGuardarChar;
	int numeroDeBloqueLibre=proximobloqueLibre();
	//ultimoBloqueChar=string_itoa(numeroDeBloque);
	sizeDelStringArchivoAGuardarChar=string_itoa(numero_lineas);
	char *actualizarBloques=malloc(strlen("[")+1);
	strcpy(actualizarBloques,"[");
	int i=0;
	int cantNFaltantes=numero_lineas;
	int cantNescribir;
	int flag = 1;
	while(flag !=0){
		ultimoBloqueChar=string_itoa(numeroDeBloqueLibre);
		bitarray_set_bit(bitarray, numeroDeBloqueLibre);
		//actualizarBitarray();
		string_append(&actualizarBloques,ultimoBloqueChar);
		if (cantNFaltantes < (i+1)*config_MetaData.tamanio_bloques){
			cantNescribir = cantNFaltantes;
			flag =0;
		}
		else{
			cantNFaltantes -= config_MetaData.tamanio_bloques;
			cantNescribir=config_MetaData.tamanio_bloques;
			string_append(&actualizarBloques,",");
			numeroDeBloqueLibre=proximobloqueLibre();
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

    if (archivo_a_guardar->ocupado_archivo_a_guardar==0){
    	memcpy(archivo_a_guardar->strig_archivo,guardado->buffer,strlen(guardado->buffer));
    	archivo_a_guardar->path=string_duplicate(complete_path);
    	archivo_a_guardar->ocupado_archivo_a_guardar=1;
    	t_config *archivo_MetaData;
    	archivo_MetaData=config_create(complete_path);
    	archivo_a_guardar->bloques=config_get_array_value(archivo_MetaData,"BLOQUES");
    	config_destroy(archivo_MetaData);
    }
    else{
    	  if(guardado->size==0){
    		    int respuesta;
    		  	if(hayEspacioParaGuardar()==0){
    		  		guardarEnArchivo();
    		  		respuesta=GUARDAR_OK;
    		  		serializarYEnviarEntero(DAM_fd,&respuesta);
    		  		return 0;
    		  	}
    		  	respuesta=GUARDAR_FALLO;
    		  	serializarYEnviarEntero(DAM_fd,&respuesta);
    	    	return -1;
    	   }
    	  else{
    		  string_append(&archivo_a_guardar->strig_archivo,guardado->buffer);
    	  }

    }
    free(guardado);
    return 0;
}

int hayEspacioParaGuardar(){
	int cantidadDeBloquesDelArchivo = cantidadDeBloques (archivo_a_guardar->bloques);
	int bloquesLibresCantidad =  cantidadDeBloquesLibres();
	int espacioDelArchivo = strlen(archivo_a_guardar->strig_archivo);
	int espacioMaximoActual =cantidadDeBloquesDelArchivo*config_MetaData.tamanio_bloques;
	int espacioMaximoLibre = bloquesLibresCantidad * config_MetaData.tamanio_bloques;
	if((espacioMaximoActual)>espacioDelArchivo){
		log_info(log_MDJ,"no se necesita espacio Adicional para guadar data de :",archivo_a_guardar->path);
		return 0;
	}
	else if (espacioMaximoLibre +espacioMaximoActual > espacioDelArchivo ){
		int espacioNecesario= espacioDelArchivo -(espacioMaximoLibre +espacioMaximoActual);
		int bloquesNecesario =cantidadDeBloquesNecesario(espacioNecesario);
		asignarleBloquesNuevosA(archivo_a_guardar,bloquesNecesario);
		log_info(log_MDJ,"no se necesita bloques adicionales para guardar el archivo :",archivo_a_guardar->path);
		return 0;
	}
	else{
		log_info(log_MDJ,"ho hay espacio para guardar el archivo:",archivo_a_guardar->path);
		free(archivo_a_guardar->strig_archivo);
		free(archivo_a_guardar->path);
		free(archivo_a_guardar->bloques);
		archivo_a_guardar->ocupado_archivo_a_guardar=0;
		return -1;
	}


}

int cantidadDeBloquesNecesario(int espacioFaltante){
	int i=1;
	while (i*config_MetaData.tamanio_bloques<espacioFaltante){
		i++;
	}
	return i;
}

void asignarleBloquesNuevosA(t_config_archivo_a_guardar *archivo_a_guardar,int bloquesNecesario){
	t_config *archivo_MetaData;
	archivo_MetaData=config_create(archivo_a_guardar->path);
	char **bloques=config_get_array_value(archivo_MetaData,"BLOQUES");

	int cantBloques=cantidadDeBloques(bloques);
	int i;
	char *actualizarBloques=malloc(strlen("[")+1);
	strcpy(actualizarBloques,"[");
	string_append(&actualizarBloques,bloques[0]);
	for(i=0;i<cantBloques-1; i++){
		puts(bloques[i]);
		string_append(&actualizarBloques,",");
		string_append(&actualizarBloques,bloques[i]);
	}
	int suguienteBloqueLibre;
	for(i=0;i<bloquesNecesario;i++){
		string_append(&actualizarBloques,",");
		suguienteBloqueLibre =proximobloqueLibre();
		char *ultimoBloqueChar;
		ultimoBloqueChar= string_itoa(suguienteBloqueLibre);
		bitarray_set_bit(bitarray,suguienteBloqueLibre);
		string_append(&actualizarBloques,ultimoBloqueChar);
	}
    string_append(&actualizarBloques,"]");
    config_set_value(archivo_MetaData, "Bloques",actualizarBloques);
    char *tamanio=string_itoa(strlen(archivo_a_guardar->strig_archivo));
    config_set_value(archivo_MetaData, "Tamanio",tamanio);
	config_destroy(archivo_MetaData);

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
			//char * path = "/scripts/checkpoint.escriptorio";
			//path: /equipos/equipo1.txt	offset: 0	size: 16
			//char * path = "fer/2.txt";
			char *path="equipos/equipo1.txt";
			//crearDirectorio(path);
			//crearArchivo(path,70);
			borrar_archivo(path);
			//peticion_obtener *obtener = malloc (sizeof(peticion_obtener));
			//obtener->offset=0;
			//obtener->path="equipos/equipo1.txt";

			//obtener->size=16;
			//crearStringDeArchivoConBloques(obtener);

			/*int creacion = CREAR_OK;
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
			*/
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
	int cantidadBloques =cantidadDeBloques (metadataArchivo.bloques);
	char * contenidoArchivo = (char *) malloc(metadataArchivo.tamanio+1);
	config_destroy(archivo_MetaData);
	int sizeArchivoBloque;
	struct stat statbuf;
	int i;
	char *src;
	char *pathBloqueCompleto;

	for(i=0;i<cantidadBloques;i++){
		pathBloqueCompleto = bloque_path(metadataArchivo.bloques[i]);
		if(metadataArchivo.tamanio >=config_MetaData.tamanio_bloques){
			sizeArchivoBloque = config_MetaData.tamanio_bloques;
		}
		else{
			sizeArchivoBloque = metadataArchivo.tamanio;
		}


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
		copiarHasta = metadataArchivo.tamanio -desplazamiento_archivo ;
		char *sub3;
		sub3=(char*)malloc(copiarHasta+1);
		//=substring(contenidoArchivo,desplazamiento_archivo, copiarHasta+1);
		memcpy(sub3, contenidoArchivo+desplazamiento_archivo,copiarHasta);
		sub3[copiarHasta] = '\0';
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
    	t_config *archivo_MetaData;
    	archivo_MetaData=config_create(complete_path);
    	t_config_MetaArchivo metadataArchivo;
    	metadataArchivo.bloques=config_get_array_value(archivo_MetaData,"BLOQUES");
    	int cantidadBloques =cantidadDeBloques (metadataArchivo.bloques);
    	config_destroy(archivo_MetaData);
    	char *pathBloqueCompleto;
    	int i;
    	int bloquelibre;
    	for(i=0;i<cantidadBloques;i++){
    		pathBloqueCompleto = bloque_path(metadataArchivo.bloques[i]);
    		log_info(log_MDJ,"liberado bloque n°:",metadataArchivo.bloques[i]);
    		//Si el bloque no llega a existir lo crea y lo deja en blanco
    		//aunque no va a par
    		fclose(fopen(pathBloqueCompleto, "w"));
    		free(pathBloqueCompleto);
    		bloquelibre= atoi(metadataArchivo.bloques[i]);
    		bitarray_clean_bit(bitarray, bloquelibre);
    	}
        if(validarArchivoConfig(complete_path)==0){
                remove(complete_path);
                log_info(log_MDJ,"borrado archivo en:",complete_path);
                free(complete_path);

                return 0;
        }
        else
                return -1;

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
		if (validarArchivoConfig(pathBloqueCompleto)<0) {
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
	t_config *archivo_MetaData;
	archivo_MetaData=config_create(archivo_a_guardar->path);
	t_config_MetaArchivo metadataArchivo;
	metadataArchivo.tamanio=config_get_int_value(archivo_MetaData,"TAMANIO");
	metadataArchivo.bloques=config_get_array_value(archivo_MetaData,"BLOQUES");
	int bloquesCantidad=cantidadDeBloques (metadataArchivo.bloques);
	int sizeDelStringArchivoAGuardar = strlen(archivo_a_guardar->strig_archivo);
	int sizeGuardar;
	for(int i=0;i<bloquesCantidad;i++){
		int offset=0;
		char *pathBloqueCompleto;
		pathBloqueCompleto = bloque_path(metadataArchivo.bloques[i]);
		offset=i*config_MetaData.tamanio_bloques;
		if(sizeDelStringArchivoAGuardar < (i+1)*config_MetaData.tamanio_bloques){
		    	sizeGuardar=sizeDelStringArchivoAGuardar;
		}
		else{
		    	sizeDelStringArchivoAGuardar-=config_MetaData.tamanio_bloques;
		}
		char *guardar = malloc(sizeGuardar +1);
		strncpy(guardar, archivo_a_guardar->strig_archivo+offset, sizeGuardar);
		guardarEnbloque(pathBloqueCompleto,guardar);
		free(guardar);
		free(pathBloqueCompleto);
	}
}

int guardarEnbloque(char *path,char *string){
	  log_info(log_MDJ,"guardar en bloque:",path);
	  int fd = open(path, O_RDWR);
	  const char *text = string;
	  size_t textsize = strlen(text) + 1;
	  char *map = mmap(0, textsize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	  memcpy(map, text, strlen(text));
	  msync(map, textsize, MS_SYNC);
	  munmap(map, textsize);
	  close(fd);
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

char *path_bitmap(){
	char *direccionArchivoBitMap=(char *) malloc(1 + strlen(config_MDJ.mount_point) + strlen("/Metadata/Bitmap.bin"));
	strcpy(direccionArchivoBitMap,config_MDJ.mount_point);
	string_append(&direccionArchivoBitMap,"/Metadata/Bitmap.bin");
	puts(direccionArchivoBitMap);
	return direccionArchivoBitMap;

}

void crearBitmap(){
	//FILE* bloque_crear;
	//bloque_crear->_IO_buf_base
	char *direccionArchivoBitMap = path_bitmap();
	int bitmap = open(direccionArchivoBitMap, O_RDWR);
	struct stat mystat;
	//puts(bitmap);
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
	size_t	cantidadDebits= bitarray_get_max_bit (bitarray);
	for (int i=0;i<cantidadDebits;i++){
		printf("posicion %d valor %d:\n",i,bitarray_test_bit(bitarray,i));
	}



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
/*
int verificar_bloques(char *path){
	t_config *archivo_MetaData;
	archivo_MetaData=config_create(path);
	t_config_MetaArchivo metadataArchivo;
	metadataArchivo.tamanio=config_get_int_value(archivo_MetaData,"TAMANIO");
	metadataArchivo.bloques=config_get_array_value(archivo_MetaData,"BLOQUES");
	int cantidadBloques=sizeof(metadataArchivo.bloques)+1;
	char * contenidoArchivo = (char *) malloc(metadataArchivo.tamanio+1);
	int sizeArchivoBloque;
	struct stat statbuf;
}

*/
int cantidadDeBloques (char **bloque){
	int i=0;
	while (bloque[i]!=0){
		i++;
	}
	return i;
}

int cantidadDeBloquesLibres (){
	size_t	cantidadDebits= bitarray_get_max_bit (bitarray);
	int libre =0;
	int i;
	for (i=0;i<cantidadDebits;i++){
		if (bitarray_test_bit(bitarray,i)==0){
			libre++;
		}
	}
	return libre;
}
int proximobloqueLibre (){
	size_t	cantidadDebits= bitarray_get_max_bit (bitarray);
	int i;
	int libre=0;
	for (i=0;i<cantidadDebits;i++){
		if(bitarray_test_bit(bitarray,i)==0){
			libre=i;
			break;
		}
	}
	return libre;
}

int hayEspacio(peticion_crear *crear){
	int espaciolibre = cantidadDeBloquesLibres()*config_MetaData.tamanio_bloques;
	int espacioNecesario = crear->cant_lineas;
	if(espaciolibre>espacioNecesario){
		return 0;
	}
	return -1;
}


int creacionDeArchivoBitmap(char *path,int cantidad){
    int x = 0;
    FILE *fh = fopen (path, "wb");
    for(int i=0;i<cantidad;i++){
        if (fh != NULL) {
                fwrite (&x, sizeof (x), 1, fh);
        }
    }
    fclose(fh);
    return 0;

}

void crearDirectoriofileSystem(char *directorio){
	char *direccionDirectorio=(char *) malloc(1 + strlen(config_MDJ.mount_point) + strlen(directorio));
	strcpy(direccionDirectorio,config_MDJ.mount_point);
	string_append(&direccionDirectorio,directorio);
	if (mkdir(direccionDirectorio, S_IRWXU) != 0) {
	    if (errno != EEXIST)
	   	        	printf( "No se creo el directorio por q ya existe%s\n",direccionDirectorio);
	}
}

void crearEstructura(){
	char *directorio="/Directorio";
	crearDirectoriofileSystem(directorio);
	directorio="/Bloque";
	crearDirectoriofileSystem(directorio);
	if (validarArchivoConfig(path_bitmap())<0){
			log_info(log_MDJ,"creacion del archivo bitmap");
			creacionDeArchivoBitmap(path_bitmap(),config_MetaData.cantidad_bloques);
	}
}
/*
void actualizarBitarray(){
	char * direccionBitmap = path_bitmap();
}
*/
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


