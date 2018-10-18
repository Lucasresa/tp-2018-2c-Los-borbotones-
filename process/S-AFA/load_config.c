#include "load_config.h"

void crear_colas(){

	cola_new = queue_create();
	cola_ready = queue_create();
	cola_block = dictionary_create();
	cola_exec = dictionary_create();
	cola_exit = queue_create();

}

void load_config(void){

	file_SAFA=config_create("CONFIG_S-AFA.cfg");

	config_SAFA.port=config_get_int_value(file_SAFA,"PUERTO");
	config_SAFA.algoritmo=detectarAlgoritmo(config_get_string_value(file_SAFA,"ALGORITMO"));
	config_SAFA.quantum=config_get_int_value(file_SAFA,"QUANTUM");
	config_SAFA.multiprog=config_get_int_value(file_SAFA,"MULTIPROGRAMACION");
	config_SAFA.retardo=config_get_int_value(file_SAFA,"RETARDO_PLANIF");

	config_destroy(file_SAFA);
}

//Funcion para detectar el algoritmo de planificacion
t_algoritmo detectarAlgoritmo(char*algoritmo){

	t_algoritmo algo;

	if(string_equals_ignore_case(algoritmo,"RR")){
		algo=RR;
	}else if(string_equals_ignore_case(algoritmo,"VRR")){
		algo=VRR;
	}else if(string_equals_ignore_case(algoritmo,"PROPIO")){
		algo=PROPIO;
	}else{
		algo=FIFO;
	}
	return algo;
}
// Se mantiene alerta de cambios en el archivo de configuracion y si se producen.. lo actualiza

void actualizar_file_config()
{
	pthread_mutex_init(&lock,NULL);
	char buffer[BUF_LEN];
	char* directorio = "/home/utnso/Escritorio/tp-2018-2c-Los-borbotones-/process/S-AFA";
	while(1){
	//Creando fd para el inotify
	int file_descriptor = inotify_init();
	if (file_descriptor < 0) {
		log_error(log_SAFA,"Error inotify_init");
	}
	// Creamos un monitor sobre el path del archivo de configuracion indicando que eventos queremos escuchar
	int watch_descriptor = inotify_add_watch(file_descriptor,directorio , IN_MODIFY);

	// Leemos la informacion referente a los eventos producidos en el file
	int length = read(file_descriptor, buffer, BUF_LEN);

	if (length < 0) {
		log_error(log_SAFA,"Error al leer cambios");
	}

	int offset = 0;

	while (offset < length) {

		struct inotify_event *event = (struct inotify_event *) &buffer[offset];

		// El campo "len" nos indica la longitud del tamaño del nombre
		if (event->len) {
			// Dentro de "mask" tenemos el evento que ocurrio y sobre donde ocurrio

			if (event->mask & IN_MODIFY) {
				if (event->mask & IN_ISDIR)
				{

				}
				else
				{
					pthread_mutex_lock(&lock);
					load_config();
					pthread_mutex_unlock(&lock);
				}
			}
		}
		offset += sizeof (struct inotify_event) + event->len;
	}

	inotify_rm_watch(file_descriptor, watch_descriptor);
	close(file_descriptor);
	pthread_mutex_destroy(&lock);
}

}

