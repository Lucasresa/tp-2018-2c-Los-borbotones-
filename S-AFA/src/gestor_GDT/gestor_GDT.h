#ifndef PROCESS_S_AFA_GESTOR_G_DT_GESTOR_G_DT_H_
#define PROCESS_S_AFA_GESTOR_G_DT_GESTOR_G_DT_H_

#include <readline/readline.h>
#include <readline/history.h>
#include <commons/string.h>
#include <stdlib.h>
#include <commons/log.h>
#include <commons/config.h>
#include <commons/collections/queue.h>
#include <commons/collections/list.h>
#include <commons/collections/dictionary.h>
#include <stdbool.h>
#include <pthread.h>
#include "../../../biblioteca/bibliotecaDeSockets.h"

typedef struct{

	int port;
	t_algoritmo algoritmo;
	int quantum;
	int multiprog;
	int retardo;

}t_config_SAFA;

typedef struct{

	int id_dtb;
	int sent_NEW;
	int sent_ejecutadas;
	int sent_DAM;
	int tiempo_respuesta;

}t_metricas;

typedef struct{
	int valor;
	t_list* cola_bloqueados;
}t_semaforo;

typedef enum{
	STATUS,
	FINALIZAR
}operaciones;

void* consola_SAFA(void);
char** parsearLinea(char* linea);
int identificarProceso(char ** linea_parseada);
void ejecutarComando(int nro_op , char * args);

char* buscarDTB(t_DTB** ,int ,int );
t_DTB* buscarDTBEnCola(t_list* ,int ,int );
t_DTB* getDTBEnCola(t_list* ,int );

t_metricas* buscarMetricasDTB(int);
void actualizarMetricaDTB(int , int );
void actualizarMetricasDTBNew();
int getSentenciasDAM();
int getSentenciasTotales();
int getSentenciasParaExit();

void actualizarTiempoDeRespuestaDTB();
int validarTiempoDeRespuesta(int );
float getTiempoDeRespuestaPromedioDTB(int );
float getTiempoDeRespuestaPromedioSistema();

int getCantidadProcesosInicializados(t_list* );

void agregarDTBDummyANew(char*,t_DTB*);
void ejecutarPLP();
void ejecutarPCP(int, t_DTB*);
void ejecutarProceso(t_DTB*,int);

void algoritmo_FIFO_RR(t_DTB* dtb);
void algoritmo_VRR(t_DTB* dtb);
void algoritmo_PROPIO(t_DTB* dtb);


t_config* file_SAFA;
t_config_SAFA config_SAFA;

t_list* cola_new;
t_list* cola_ready;
t_list* cola_ready_VRR;
t_list* cola_ready_IOBF;

t_list* cola_block;
t_dictionary* cola_exec;

t_list* cola_exit;


t_log* log_SAFA;

t_list* CPU_libres;

t_dictionary* claves;
t_list* show_claves;

pthread_mutex_t mx_PCP;
pthread_mutex_t mx_PLP;

pthread_mutex_t mx_colas;
pthread_mutex_t mx_CPUs;

pthread_mutex_t mx_claves;

pthread_mutex_t mx_metricas;

pthread_mutex_t File_config;

pthread_mutex_t mx_semaforos;

t_dictionary* semaforos_dtb;

t_list* info_metricas;

int multiprogramacion_actual;

int DAM;

#endif /* PROCESS_S_AFA_GESTOR_G_DT_GESTOR_G_DT_H_ */
