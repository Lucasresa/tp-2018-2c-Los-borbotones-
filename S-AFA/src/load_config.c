#include "load_config.h"

void crear_colas(){

	cola_new = list_create();
	cola_ready = list_create();
	cola_ready_VRR = list_create();
	cola_ready_IOBF = list_create();
	cola_block = dictionary_create();
	cola_exec = dictionary_create();
	cola_exit = list_create();

}

void iniciar_semaforos(){

	pthread_mutex_init(&mx_CPUs,NULL);
	pthread_mutex_init(&mx_PCP,NULL);
	pthread_mutex_init(&mx_PLP,NULL);
	pthread_mutex_init(&mx_claves,NULL);
	pthread_mutex_init(&mx_colas,NULL);

}

void load_config(void){

	file_SAFA=config_create("src/CONFIG_S-AFA.cfg");

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
	}else if(string_equals_ignore_case(algoritmo,"IOBF")){
		algo=IOBF;
	}else{
		algo=FIFO;
	}
	return algo;
}
// Se mantiene alerta de cambios en el archivo de configuracion y si se producen.. lo actualiza

void actualizar_file_config()
{
	pthread_mutex_init(&File_config,NULL);
	char buffer[BUF_LEN];
	char* directorio = "src/CONFIG_S-AFA.cfg";
	while(1){
	//Creando fd para el inotify
	int file_descriptor = inotify_init();
	if (file_descriptor < 0) {
		log_error(log_SAFA,"Error inotify_init");
	}
	// Creamos un monitor sobre el path del archivo de configuracion indicando que eventos queremos escuchar
	int watch_descriptor = inotify_add_watch(file_descriptor,directorio , IN_MODIFY);

	if (watch_descriptor < 0)
	{
		log_error(log_SAFA,"Error al crear el monitor..");
	}

	// Leemos la informacion referente a los eventos producidos en el file
	int length = read(file_descriptor, buffer, BUF_LEN);

	if (length < 0) {
		log_error(log_SAFA,"Error al leer cambios");
	}

	int offset = 0;

	while (offset < length) {

		struct inotify_event *event = (struct inotify_event *) &buffer[offset];

		// Dentro de "mask" tenemos el evento que ocurrio y sobre donde ocurrio
		if (event->mask & IN_MODIFY) {
				//Actualizo el archivo de configuracion
				pthread_mutex_lock(&File_config);
				load_config();
				pthread_mutex_unlock(&File_config);
				log_warning(log_SAFA,"Se modifico el archivo de configuracion");
		}
		offset += sizeof (struct inotify_event) + event->len;
		}
	pthread_mutex_destroy(&File_config);
	//Quito el reloj del directorio
	inotify_rm_watch(file_descriptor, watch_descriptor);
	//Cierro el fd
	close(file_descriptor);
	}

}

