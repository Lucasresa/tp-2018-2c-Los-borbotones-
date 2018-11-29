#include "CPU.h"

int main(){

	//Creo un log para el CPU

	log_CPU = log_create("CPU.log","CPU",true,LOG_LEVEL_INFO);

	file_CPU=config_create("src/CONFIG_CPU.cfg");

	config_CPU.ip_safa=string_duplicate(config_get_string_value(file_CPU,"IP_SAFA"));
	config_CPU.puerto_safa=config_get_int_value(file_CPU,"PUERTO_SAFA");
	config_CPU.ip_diego=string_duplicate(config_get_string_value(file_CPU,"IP_DIEGO"));
	config_CPU.puerto_diego=config_get_int_value(file_CPU,"PUERTO_DIEGO");
	config_CPU.ip_fm9=string_duplicate(config_get_string_value(file_CPU,"IP_FM9"));
	config_CPU.puerto_fm9=config_get_int_value(file_CPU,"PUERTO_FM9");
	config_CPU.retardo=config_get_int_value(file_CPU,"RETARDO");

	config_destroy(file_CPU);
/*
	printf("ip safa: %s\tpuerto safa: %d\tip diego: %s\tpuerto diego: %d\tretardo: %d\n",
				config_CPU.ip_safa,config_CPU.puerto_safa,config_CPU.ip_diego,config_CPU.puerto_diego,config_CPU.retardo);
*/

	int SAFA_fd,DAM_fd,FM9_fd;

	crearSocket(&SAFA_fd);
	crearSocket(&DAM_fd);
	crearSocket(&FM9_fd);

	//El CPU se conecta con SAFA
	if(conectar(&SAFA_fd,config_CPU.puerto_safa,config_CPU.ip_safa)!=0){
		log_error(log_CPU,"Error al conectarse con SAFA");
		exit(1);
	}

	log_info(log_CPU,"Conexion exitosa con SAFA");


	//El CPU se conecta con "el diego"
//	if(conectar(&DAM_fd,config_CPU.puerto_diego,config_CPU.ip_diego)!=0){
//		log_error(log_CPU,"Error al conectarse con DAM");
//		exit(1);
//	}
//
//	if(conectar(&FM9_fd,config_CPU.puerto_fm9,config_CPU.ip_fm9)!=0){
//		log_error(log_CPU,"Error al conectarse con FM9");
//		exit(1);
//	}


	while(1){

		if(recv(SAFA_fd,&rafaga_recibida,sizeof(int),0)<=0){
			perror("Error al recibir la rafaga de planificacion");
			exit(1);
		}

		//Espero para recibir un DTB a ejecutar

		log_info(log_CPU,"Esperando DTB del S-AFA");

		t_DTB dtb=RecibirYDeserializarDTB(SAFA_fd);

		serializarYEnviarEntero(SAFA_fd,&dtb.id);

		log_info(log_CPU,"DTB Recibido con ID: %d",dtb.id);

		rafaga_actual=rafaga_recibida;

		if(dtb.f_inicializacion==0){
			log_info(log_CPU,"El DTB tiene su flag en 0, comenzando inicializacion...");
			inicializarDTB(DAM_fd,SAFA_fd,&dtb);
		}else{
			log_info(log_CPU,"El DTB tiene su flag en 1, comenzando con la ejecucion...");
			comenzarEjecucion(SAFA_fd,DAM_fd,FM9_fd,dtb);
		}
	}

	return 0;
}

void inicializarDTB(int DAM_fd,int SAFA_fd,t_DTB* dtb){

	void*buffer;
	int protocolo=BLOQUEAR_PROCESO;
	//Enviar peticion de "abrir" al MDJ por medio de "el diego"
	//desbloqueo_dummy dummy = {.path = string_duplicate(dtb->escriptorio), .id_dtb = dtb->id};

	//serializarYEnviar(DAM_fd,DESBLOQUEAR_DUMMY,&dummy);

	//Una vez enviando el dummy a DAM tengo que avisarle a SAFA que bloquee el proceso mientras se carga en memoria el script

	notificarSAFA(SAFA_fd,protocolo,*dtb);
	usleep(config_CPU.retardo*1000);

}

void comenzarEjecucion(int SAFA, int DAM, int FM9, t_DTB dtb){

	/*
	 * Primero tengo que pedirle al fm9 las lineas para ejecutar
	 * Una vez que recibo las lineas, tengo que parsearlas con mi parser
	 * Luego comunicarme con DAM y enviarle algo dependiendo de la instruccion que se este ejecutando
	 * En caso de ser una I/O debo avisar a SAFA y desalojar el DTB para poder ejecutar otro
	 * Por cada linea que se ejecuta debo decrementar la rafaga_actual, cuando esta llegue a 0 hay que informar a SAFA
	*/

	int interrupcion=0;

	direccion_logica* direccion=malloc(sizeof(direccion_logica));

	int protocolo;

	if(dtb.quantum_sobrante!=0){
		rafaga_actual=dtb.quantum_sobrante;
	}


	do{

		direccion->pid=dtb.id;
		direccion->base=0;
		direccion->offset=dtb.pc;

		serializarYEnviar(FM9,PEDIR_DATOS,direccion);

		char* linea_fm9 = recibirYDeserializarString(FM9);

		t_operacion linea_parseada=parseLine(linea_fm9);

		switch(linea_parseada.keyword){
		case ABRIR:

			if(isOpenFile(dtb,linea_parseada.argumentos.abrir.path))
				break;
			else{
				protocolo=ABRIR_ARCHIVO;
				serializarYEnviarEntero(DAM,&protocolo);
				serializarYEnviarString(DAM,linea_parseada.argumentos.abrir.path);
				notificarSAFA(SAFA,BLOQUEAR_PROCESO,dtb);
				interrupcion=1;
			}
			break;
		case CONCENTRAR:
			break;
		case ASIGNAR:
			break;
		case WAIT:
			notificarSAFA(SAFA,WAIT_RECURSO,dtb);
			serializarYEnviarString(SAFA,linea_parseada.argumentos.wait.recurso);
			int* resultado_wait=recibirYDeserializarEntero(SAFA);
			if(*resultado_wait!=WAIT_EXITOSO){
				interrupcion=1;
			}
			break;
		case SIGNAL:
			notificarSAFA(SAFA,SIGNAL_RECURSO,dtb);
			serializarYEnviarString(SAFA,linea_parseada.argumentos.signal.recurso);
			int* resultado_signal=recibirYDeserializarEntero(SAFA);
			if(*resultado_signal!=SIGNAL_EXITOSO){
				interrupcion=1;
			}
			break;
		case FLUSH:
			break;
		case CLOSE:

			if(isOpenFile(dtb,linea_parseada.argumentos.close.path)){
				//Mandar a FM9 la informacion necesaria para que libere la memoria de dicho archivo
				//Luego hay que actualizar la lista de archivos abiertos del dtb
			}else{
				notificarSAFA(SAFA,FINALIZAR_PROCESO,dtb);
				interrupcion=1;
			}
			break;
		case CREAR:
			protocolo=CREAR_ARCHIVO;
			serializarYEnviarEntero(DAM,&protocolo);
			serializarYEnviarString(DAM,linea_parseada.argumentos.crear.path);
			serializarYEnviarEntero(DAM,&(linea_parseada.argumentos.crear.lineas));
			notificarSAFA(SAFA,BLOQUEAR_PROCESO,dtb);
			interrupcion=1;
			break;
		case BORRAR:
			break;
		default:
			log_info(log_CPU,"Se termino de ejecutar el script");
			notificarSAFA(SAFA,FINALIZAR_PROCESO,dtb);
			interrupcion=1;
			break;
		}

		actualizarDTB(&dtb);

		if(rafaga_recibida!=0){
			if(rafaga_actual==0){
				notificarSAFA(SAFA,FIN_QUANTUM,dtb);
				return;
			}
		}
		destroyParse(linea_parseada);

		log_warning(log_CPU, "Instruccion ejecutada, quantum restante = %d",dtb.quantum_sobrante);

		usleep(config_CPU.retardo*1000);
	}while(!interrupcion);


}

void notificarSAFA(int SAFA,int protocolo, t_DTB DTB){

	void*buffer;

	send(SAFA,&protocolo,sizeof(int),0);

	serializarYEnviarDTB(SAFA,buffer,DTB);

}

void actualizarDTB(t_DTB* dtb){

	if(rafaga_recibida!=0){
		rafaga_actual--;

		dtb->pc++;
		dtb->quantum_sobrante=rafaga_actual;
	}
}

int isOpenFile(t_DTB dtb, char* path){

	int i, retorno = 0;

	int cant_archivos = list_size(dtb.archivos);

	t_archivo* archivo;

	for(i=0;i<cant_archivos;i++){

		archivo=list_get(dtb.archivos,i);

		if(strcmp(archivo->path,path)==0){
			retorno = 1;
			break;
		}

	}

	return retorno;
}
