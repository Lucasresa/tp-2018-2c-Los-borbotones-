#ifndef BIBLIOTECADESOCKETS_H_
#define BIBLIOTECADESOCKETS_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <commons/collections/list.h>
#include <commons/string.h>
#include <fcntl.h>
#include <commons/log.h>

//Estructura para manejar el protocolo
typedef enum{
	VALIDAR_ARCHIVO,	//PARA EL MDJ
	CREAR_ARCHIVO,		//PARA EL MDJ
	OBTENER_DATOS,		//PARA EL MDJ
	GUARDAR_DATOS,		//PARA EL MDJ
	BORRAR_ARCHIVO,		//PARA EL MDJ
	EJECUTAR_PROCESO,	//PARA EL SAFA
	FINALIZAR_PROCESO, 	//PARA CPU Y SAFA
	BLOQUEAR_PROCESO,	//PARA CPU y SAFA
	DESBLOQUEAR_PROCESO,//PARA SAFA Y DAM
	FIN_QUANTUM,		//PARA CPU Y SAFA
	DESBLOQUEAR_DUMMY,	//PARA CPU Y DAM
	ENVIAR_ARCHIVO,     //PARA DAM Y MDJ/FM9
	FINAL_CARGA_DUMMY,	//PARA SAFA Y DAM
	FINAL_ABRIR,		//PARA SAFA Y DAM
	FINAL_CREAR,        //PARA SAFA Y DAM
	INICIAR_MEMORIA_PID,//Para el DAM al FM9
	MEMORIA_INICIALIZADA,
	INICIAR_SCRIPTORIO,
	PEDIR_DATOS,
	ABRIR_ARCHIVO
}t_protocolo;

enum paquete {
	PAQ_INT,
	PAQ_STRING,
	PAQ_PCB,
	PAQ_MPROCESO,
	PAQ_PETICION_PAGINA_SWAP,
	PAQ_LECTURA_CONTENIDO,
	PAQ_RESPUESTA_OPERACION,
};

enum resultado{
	CORRECTO,
	ERROR,
};

typedef struct {
	int pid;
	int base;
	int offset;
	char* linea;
} cargar_en_memoria;

struct mProc {
	int PID;
	char* rutaRelativaDeArchivo;
	int estadoDelProceso;
	int PC;
} mProc;

struct peticionLecturaSwap{
	int idProceso;
	int numeroPagina;
};

struct mProcesoSwap{
	int id;
	int cantidadTotalPaginas;
	int framesAsignados[];//TODO sacar vector (con el frame inicial y la cantidad de paginas alcanza
};

struct peticionLineaTLB{
	int id_proceso;
	int dir_logica;
	int dir_fisica;
	int tiempo;
};

struct paqContenido{
	char* contenido;
};



struct respuesta{
	enum resultado res;
};

typedef struct{
	char*path;
}peticion_validar;

typedef struct{
	char*path;
	int cant_lineas;
}peticion_crear;

typedef struct{
	char*path;
	int offset;
	int size;
}peticion_obtener;

typedef struct{
	char*path;
}peticion_borrar;

typedef struct{
	char*path;
}peticion_abrir;

typedef struct{
	char*path;
	int offset;
	int size;
	char*buffer;
}peticion_guardar;

typedef struct{
	char*path;
	int id_dtb;
}desbloqueo_dummy;

typedef struct{
	int numero_tabla;
	int segmento;
	int offset;
}direccion_logica;

typedef enum{
	FIFO,
	RR,
	VRR,
	PROPIO
}t_algoritmo;

typedef struct{
	char* path;
	int acceso;
}t_archivo;

typedef struct{

	int id;
	int pc;
	int quantum_sobrante;
	int f_inicializacion;
	char* escriptorio;
	t_list* archivos;
}__attribute__((packed)) t_DTB;

t_log* log_s;


int crearSocket(int *mySocket);

int conectar(int* mySocket, int puerto, char *ip);

int setearParaEscuchar(int *mySocket, int puerto);

int aceptarConexion(int);

int escuchar(int socket, fd_set *listaDeSockets,
	void *(*funcionParaSocketNuevo)(int, void*), void *parametrosParaSocketNuevo,
	void *(*funcionParaSocketYaConectado)(int, void*),
	void *parametrosParaSocketYaConectado);

void* recibirYDeserializar(int socket,int tipo);
char* recibirYDeserializarString(int socket);
int* recibirYDeserializarEntero(int socket);

void serializarYEnviar(int socket, int tipoDePaquete, void* package);
void serializarYEnviarString(int socket, char *string);
void serializarYEnviarEntero(int socket, int *numero);

int enviarTodo(int socketReceptor, void *buffer, int *cantidadTotal);

int validarArchivoConfig(char *archivo);
void serializarYEnviarDTB(int,void*,t_DTB);
t_DTB RecibirYDeserializarDTB(int);

#endif /* BIBLIOTECADESOCKETS_H_ */
