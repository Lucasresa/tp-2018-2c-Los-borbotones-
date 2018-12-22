#include "load_config.h"

void crear_colas(){

	cola_new = list_create();
	cola_ready = list_create();
	cola_ready_VRR = list_create();
	cola_ready_IOBF = list_create();
	cola_block = list_create();
	cola_exec = dictionary_create();
	cola_exit = list_create();

}

void iniciar_semaforos(){

	pthread_mutex_init(&mx_CPUs,NULL);
	pthread_mutex_init(&mx_PCP,NULL);
	pthread_mutex_init(&mx_PLP,NULL);
	pthread_mutex_init(&mx_claves,NULL);
	pthread_mutex_init(&mx_colas,NULL);
	pthread_mutex_init(&mx_metricas,NULL);
	pthread_mutex_init(&File_config,NULL);
	pthread_mutex_init(&mx_semaforos,NULL);
	pthread_mutex_init(&lock_CPUs,NULL);
}

void load_config(void){

	file_SAFA=config_create("src/CONFIG_S-AFA.cfg");

	config_SAFA.port=config_get_int_value(file_SAFA,"PUERTO");
	config_SAFA.algoritmo=detectarAlgoritmo(config_get_string_value(file_SAFA,"ALGORITMO"));
	config_SAFA.quantum=config_get_int_value(file_SAFA,"QUANTUM");
	if(config_SAFA.algoritmo==FIFO){
		config_SAFA.quantum=0;
	}
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
		pthread_mutex_lock(&File_config);

		struct inotify_event *event = (struct inotify_event *) &buffer[offset];

		// Dentro de "mask" tenemos el evento que ocurrio y sobre donde ocurrio
		if (event->mask & IN_MODIFY) {
				//Actualizo el archivo de configuracion

				int anterior=config_SAFA.algoritmo, grado_anterior=config_SAFA.multiprog, diferencia;

				load_config();

				actualizarColas(anterior);

				if(config_SAFA.multiprog>=grado_anterior){
					diferencia = config_SAFA.multiprog-grado_anterior;
					multiprogramacion_actual+=diferencia;
				}
				else{
					diferencia = abs(config_SAFA.multiprog-grado_anterior);
					if(multiprogramacion_actual>=diferencia){
						multiprogramacion_actual-=diferencia;
					}else{
						multiprogramacion_actual=0;
					}
				}

				pthread_mutex_unlock(&File_config);

				pthread_mutex_lock(&mx_PLP);
				ejecutarPLP();
				pthread_mutex_unlock(&mx_PLP);
				log_warning(log_SAFA,"Se modifico el archivo de configuracion");
		}
		offset += sizeof (struct inotify_event) + event->len;
		}
	//Quito el reloj del directorio
	inotify_rm_watch(file_descriptor, watch_descriptor);
	//Cierro el fd
	close(file_descriptor);
	}

}


void actualizarColas(int anterior){

	if(anterior!=config_SAFA.algoritmo){

		if(config_SAFA.algoritmo==IOBF){

			list_add_all(cola_ready_IOBF,cola_ready_VRR);
			list_add_all(cola_ready_IOBF,cola_ready);

			list_clean(cola_ready);
			list_clean(cola_ready_VRR);

		}else{
			t_list* aux=list_duplicate(cola_ready);
			list_clean(cola_ready);

			list_add_all(cola_ready,cola_ready_VRR);
			list_add_all(cola_ready,cola_ready_IOBF);
			list_add_all(cola_ready,aux);

			list_clean(cola_ready_VRR);
			list_clean(cola_ready_IOBF);
			list_clean(aux);
			list_destroy(aux);
		}
	}
	return;

}





