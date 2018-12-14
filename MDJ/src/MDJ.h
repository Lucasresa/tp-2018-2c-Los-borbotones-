
#ifndef MDJ_H_
#define MDJ_H_
#include <commons/string.h>
#include <netdb.h> // Para getaddrinfo
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <stdio.h>
#include <sys/stat.h>
#include <ctype.h>
#include <commons/config.h>
#include <commons/collections/list.h>
#include <openssl/md5.h>
#include <pthread.h>
#include <commons/log.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "../../biblioteca/bibliotecaDeSockets.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h> /* mmap() is defined in this header */
#include <fcntl.h>
#include <commons/bitarray.h>
#include <sys/types.h>
#include <sys/stat.h>

char* server_ip;
int server_puerto;
int cerrarMDJ;
int cerrado_cosola;
int cerrado_conexion;

int DAM_fd;
int tamanio,max_linea,tam_pagina;
int socket_planificador;
int socketFD;
t_log * logger;
float retardo;
char *ip;
char *puerto;
char *dir_actual;
char *string_actual;
t_config* file_MDJ;
t_log* log_MDJ;


t_config *mdj_config;
in_port_t get_in_port(struct sockaddr *sa);

int iniciar_recibirDatos();
void crearBitmap();
void consola_MDJ();
void configure_logger();
void exit_gracefully(int return_nr);
void leerArchivoConfiguracion(t_config * archivoConfig);
void cmd_cat(char *linea);
void cmd_cd(char *linea);
void cmd_ls(char *linea);
void cmd_pwd();
void cmd_bloque();
void determinarOperacion(int operacion,int fd);
int mySocketM;
int DAM_fd;
int cmd_md5(char *linea);
int string_archivo(char *pathfile,char **contenido);
int existe_archivo(char *path_archivo);
int borrar_archivo(char *path_archivo);
char *archivo_path(char *path_archivo);
char *bloque_path(char *numeroBloque);
void conexion_DMA();
void crearStringDeArchivoConBloques(peticion_obtener *obtener);
void crearDirectorio(char *path);
void actualizar_configuracion_Metadata(int ultimoBlque);
char *string_config_metadata();
int cantidadDeBloques (char **bloque);
int proximobloqueLibre ();
int cantidadDeBloquesLibres ();
int cantidadDeBloques (char **bloque);
int hayEspacio(peticion_crear  *crear);
void actualizarBitarray();

struct addrinfo *server_info;

typedef struct{
 	int puerto_mdj;
	char* mount_point;
	int time_delay;
 }t_config_MDJ;

 typedef struct{
  	int tamanio_bloques;
 	char* magic_number;
 	int cantidad_bloques;
  }t_config_MetaData;

  typedef struct{
   	int tamanio;
  	char** bloques;
   }t_config_MetaArchivo;

   typedef struct{
	   char *strig_archivo;
	   char *path;
	   int  ocupado_archivo_a_guardar ;
	   char **bloques;
   }t_config_archivo_a_guardar;



#define TRUE 1
#define FALSE 0



t_config_archivo_a_guardar *archivo_a_guardar ;
t_config_MDJ config_MDJ;
t_config_MetaData  config_MetaData;

t_bitarray *bitarray;
int crear_carpetas();
int mkdir_p(const char *path);
int guardarDatos(peticion_guardar *guardado) ;
void guardarEnArchivo();
void crearArchivo(char *path, int numero_lineas) ;
int leerMetaData();
int inicializar();
int  conexion_dam();
int hayEspacioParaGuardar();
int actualizarARchivo(t_config_MetaArchivo *metadataArchivo,int sizeDelStringArchivoAGuardar,int ultimoBloque);
char *substring(char *string, int position, int length);
int cantidadDeBloquesNecesario(int espacioNecesario);
int guardarEnbloque(char *path,char *string);
void asignarleBloquesNuevosA(t_config_archivo_a_guardar *archivo_a_guardar,int bloquesNecesario);
#endif /* MDJ_H_ */
