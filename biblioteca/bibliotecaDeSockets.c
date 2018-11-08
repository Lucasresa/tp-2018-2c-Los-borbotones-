#include "bibliotecaDeSockets.h"

#define BACKLOG 20

int enviarTodo(int socketReceptor, void *buffer, int *cantidadTotal) {
    int total = 0;
    int faltante = *cantidadTotal;
    int n;

    while(total < *cantidadTotal) {
        n = send(socketReceptor, buffer+total, faltante, 0);
        if (n == -1) {
        	break;
        }
        total += n;
        faltante -= n;
    }
    *cantidadTotal = total;
	if(n==-1){
		return -1;
	}

	return 0;
}

int crearSocket(int *mySocket) {
	int opcion=1;

	if ((*mySocket=socket(AF_INET, SOCK_STREAM,0))==-1){
		perror("-1 al crear el socket");
		return -1;
	}
	if (setsockopt(*mySocket, SOL_SOCKET, SO_REUSEADDR, &opcion, sizeof(int))==-1){
		perror("-1 al setear las opciones del socket");
		return -1;
	}
	return 0;
}

int setearParaEscuchar(int *mySocket, int puerto) {
	struct addrinfo direccion, *infoBind = malloc(sizeof(struct addrinfo));

	memset(&direccion, 0, sizeof direccion);
	direccion.ai_family = AF_INET;
	direccion.ai_socktype = SOCK_STREAM;
	direccion.ai_flags = AI_PASSIVE;

	char *port= malloc(sizeof(char)*6);

	port = string_itoa(puerto);
	getaddrinfo(NULL, port, &direccion, &infoBind);

	free(port);
	int opcion=1;

	if(bind(*mySocket,infoBind->ai_addr, infoBind->ai_addrlen)==-1){
		perror("Fallo el bind");
		free(infoBind);
		return -1;
	}
	//ver si hay que sacar el free
	free(infoBind);
	if (listen(*mySocket, BACKLOG) ==-1){
		perror("Fallo el listen");
		return -1;
	}
	//Si en todos los procesos llamamos al logger con el mismo nombre podemos llamarlo desde la biblioteca
	//log_info(logger, " ... Escuchando conexiones ... ");
	return 0;
}

int conectar(int* mySocket, int puerto, char *ip) {
	struct addrinfo hints, *res;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	char *stringPort = string_itoa(puerto);
	getaddrinfo(ip, stringPort, &hints, &res);
	free(stringPort);
	if(connect(*mySocket, res->ai_addr, res->ai_addrlen)!=0){
		return -1;
	}
	return 0;
}

int aceptarConexion(int fd){

	struct sockaddr_in cliente;

	unsigned int len = sizeof(struct sockaddr);

	return accept(fd,(void*)&cliente,&len);

}


int escuchar(int socketListener, fd_set *fd,  void *(funcionSocketNuevo)(int, void*), void *argumentosSocketNuevo,
				void *(funcionSocketExistente)(int, void*), void *argumentosSocketExistente){

	fd_set copia = *fd;
	int socketsComunicandose=0;
	if((socketsComunicandose=select(FD_SETSIZE,&copia,NULL,NULL,NULL))==-1) {

		perror("Fallo en el select");
		return -1;
	}

	if(FD_ISSET(socketListener,&copia)) {

		int socketNuevo=0;
		if ((socketNuevo = accept(socketListener, NULL, 0)) < 0) {

			perror("Error al aceptar conexion entrante");
			return -1;
		} else {

			FD_SET(socketNuevo,fd);

			if(funcionSocketNuevo!=NULL) {
				funcionSocketNuevo(socketNuevo, argumentosSocketNuevo);
			}
		}
	}else{

		int i;
		for (i = 0; i < FD_SETSIZE; i++) {

			if (FD_ISSET(i, &copia) && i != socketListener) {
				funcionSocketExistente(i, argumentosSocketExistente);
			}
		}
	}
	return 0;
}

//Explicación: Serializamos convirtiendo a un string, de la forma <Tipo de struct>[<Tamaño elemento><Elemento>]
//Se envían de forma separada enteros y strings llamando a las funciones correspondientes
void serializarYEnviar(int socket, int tipoDePaquete, void* package){
	serializarYEnviarEntero(socket, &tipoDePaquete);

	switch(tipoDePaquete){
	case INICIAR_SCRIPTORIO:
		serializarYEnviarEntero(socket,&((cargar_en_memoria*)package)->pid);
		serializarYEnviarEntero(socket,&((cargar_en_memoria*)package)->base);
		serializarYEnviarEntero(socket,&((cargar_en_memoria*)package)->offset);
		printf("serializando y enviando %s\n",((cargar_en_memoria*)package)->linea);
		serializarYEnviarString(socket,((cargar_en_memoria*)package)->linea);
		return;
	case INICIAR_MEMORIA_PID:
		serializarYEnviarEntero(socket, (int*)&package);
		break;
	case ENVIAR_ARCHIVO:
		serializarYEnviarString(socket,(char*)package);
		break;
	case VALIDAR_ARCHIVO:
		serializarYEnviarString(socket,((peticion_validar*)package)->path);
		break;
	case CREAR_ARCHIVO:
		serializarYEnviarString(socket, ((peticion_crear*)package)->path);
		serializarYEnviarEntero(socket,&((peticion_crear*)package)->cant_lineas);
		break;
	case OBTENER_DATOS:
		serializarYEnviarString(socket,((peticion_obtener*)package)->path);
		serializarYEnviarEntero(socket,&((peticion_obtener*)package)->offset);
		serializarYEnviarEntero(socket,&((peticion_obtener*)package)->size);
		break;
	case GUARDAR_DATOS:
		serializarYEnviarString(socket,((peticion_guardar*)package)->path);
		serializarYEnviarEntero(socket,&((peticion_guardar*)package)->offset);
		serializarYEnviarEntero(socket,&((peticion_guardar*)package)->size);
		serializarYEnviarString(socket,((peticion_guardar*)package)->buffer);
		break;
	case PEDIR_DATOS:
		serializarYEnviarEntero(socket,&((direccion_logica*)package)->numero_tabla);
		serializarYEnviarEntero(socket,&((direccion_logica*)package)->segmento);
		serializarYEnviarEntero(socket,&((direccion_logica*)package)->offset);
		break;
	case BORRAR_ARCHIVO:
		serializarYEnviarString(socket,((peticion_borrar*)package)->path);
		break;
	}

}

void serializarYEnviarString(int socket, char *string){

	int largo, tam, offset = 0;
	char * serializado;

	largo = strlen(string)+1;
	serializado = malloc(sizeof(largo)+largo);
	memset(serializado,0,largo+sizeof(int));

	tam = sizeof(largo);
	memcpy(serializado+offset,&largo, tam);
	offset += tam;

	tam = largo;
	memcpy(serializado+offset,string,tam);
	offset += largo;

	enviarTodo(socket, serializado, &offset);
}

void serializarYEnviarEntero(int socket, int* entero){
	int largo, tam, offset = 0;
	char * serializado;

	largo = sizeof(entero);
	serializado = malloc(sizeof(largo)+largo);
	memset(serializado,0,largo+sizeof(largo));

	tam = sizeof(largo);
	memcpy(serializado+offset,&largo, tam);
	offset += tam;

	tam = largo;
	memcpy(serializado+offset,entero,tam);
	offset += largo;

	enviarTodo(socket, serializado, &offset);
}

void* recibirYDeserializar(int socket,int tipo){
	switch(tipo){
	case INICIAR_SCRIPTORIO:
	{
		puts("Intento ejecutar malloc.");
		cargar_en_memoria* info_a_cargar = malloc(sizeof(cargar_en_memoria));
		puts("Malloc ejecutado!");
		info_a_cargar->pid = *recibirYDeserializarEntero(socket);
		info_a_cargar->base = *recibirYDeserializarEntero(socket);
		info_a_cargar->offset = *recibirYDeserializarEntero(socket);
		info_a_cargar->linea=recibirYDeserializarString(socket);
		return info_a_cargar;
	}
	case ENVIAR_ARCHIVO:
	{
		char *string_archivo=recibirYDeserializarString(socket);
		return string_archivo;
	}
	case VALIDAR_ARCHIVO:
	{
		peticion_validar* validacion = malloc(sizeof(peticion_validar));
		validacion->path=recibirYDeserializarString(socket);
		return validacion;
	}
	case CREAR_ARCHIVO:
	{
		peticion_crear* creacion = malloc(sizeof(peticion_crear));
		creacion->path = recibirYDeserializarString(socket);
		creacion->cant_lineas = *recibirYDeserializarEntero(socket);
		return creacion;
	}
	case OBTENER_DATOS:
	{
		peticion_obtener* obtener = malloc(sizeof(peticion_obtener));
		obtener->path = recibirYDeserializarString(socket);
		obtener->offset = *recibirYDeserializarEntero(socket);
		obtener->size = *recibirYDeserializarEntero(socket);
		return obtener;
	}
	case GUARDAR_DATOS:
	{
		peticion_guardar* guardado = malloc(sizeof(peticion_guardar));
		guardado->path = recibirYDeserializarString(socket);
		guardado->offset = *recibirYDeserializarEntero(socket);
		guardado->size = *recibirYDeserializarEntero(socket);
		guardado->buffer = recibirYDeserializarString(socket);
		return guardado;
	}
	case PEDIR_DATOS:
	{
		direccion_logica* direccion = malloc(sizeof(direccion_logica));
		direccion->numero_tabla=*recibirYDeserializarEntero(socket);
		direccion->segmento=*recibirYDeserializarEntero(socket);
		direccion->offset=*recibirYDeserializarEntero(socket);
		return direccion;
	}
	case BORRAR_ARCHIVO:
	{
			peticion_borrar* borrar = malloc(sizeof(peticion_borrar));
			borrar->path = recibirYDeserializarString(socket);
			return borrar;
	}
	default:
		return NULL;
	}
}

int *recibirYDeserializarEntero(int socket){
	int offset = 0, n;
	int* tam = malloc(sizeof(int));

	if (recv(socket, tam, sizeof(int), 0) <=0){
		perror("Conexión falló");
		free(tam);
		return NULL;
	}

	int *numero= malloc(*tam);

	memset(numero,0,*tam);

	while(offset < *tam) {
		n = recv(socket, numero+offset, *tam, 0);
		if(n <=0){
			perror("Conexión falló");
			free(tam);
			return NULL;
		}
		offset +=n;
	}
	free(tam);
	return numero;
}

char *recibirYDeserializarString(int socket){

	int offset = 0, *tamString = malloc(sizeof(int)), n;

	if (recv(socket, tamString, sizeof(int), 0) <=0){
		perror("Conexión falló");
		free(tamString);
		return NULL;
	}

	char *string= malloc(*tamString);

	memset(string,0,*tamString);

	while(offset < *tamString) {
		n = recv(socket, string+offset, *tamString, 0);
		if(n <=0){
			perror("Conexión falló");
			free(tamString);
			return NULL;
		}
		offset +=n;
	}
	free(tamString);
	return string;
}

//-------------------------------------------------------------------------------------------
/*Funcion para serializar el DTB y enviarlo a traves de un socket
 *Se copia todo lo que hay dentro del DTB a un buffer, la estructura seria:
 *
 *	|Tamañobuffer|Id|Pc|Flag|TamañoScript|Script|CantidadArchivosAbiertos|TamanioArchivo1|Archivo1|...|TamanioArchivoN|ArchivoN|
 *
*/
void serializarYEnviarDTB(int fd,void* buffer,t_DTB dtb){

	int i, tamanio_info;
	int offset=0;
	int tamanio_script=strlen(dtb.escriptorio)+1;
	int cantidad_archivos = list_size(dtb.archivos);

	//Calculo el tamaño total que tendra el buffer y reservo memoria para el mismo
	int tamanio_buffer=(sizeof(int)*7)+(sizeof(char)*tamanio_script);

	//Reservo memoria para cada archivo abierto si es que lo hay
	if(cantidad_archivos!=0){

		for(i=0;i<cantidad_archivos;i++){
			t_archivo* archivo=list_get(dtb.archivos,i);
			tamanio_buffer+=((sizeof(char)*strlen(archivo->path))+1);
			tamanio_buffer+=sizeof(archivo->acceso);
		}

		tamanio_buffer+=(cantidad_archivos*sizeof(int));

	}

	tamanio_info=tamanio_buffer-sizeof(tamanio_buffer);

	buffer=malloc(tamanio_buffer);

	memset(buffer,0,tamanio_buffer);

	//--------------------------------------------------------------------

	//Tamaño de la informacion del buffer

	memcpy(buffer,&tamanio_info,sizeof(tamanio_info));
	offset+=sizeof(tamanio_info);

	//Id del DTB
	memcpy(buffer+offset,&dtb.id,sizeof(dtb.id));
	offset+=sizeof(dtb.id);

	//Program counter del DTB
	memcpy(buffer+offset,&dtb.pc,sizeof(dtb.pc));
	offset+=sizeof(dtb.pc);

	//Quantum_restante del DTB
	memcpy(buffer+offset,&dtb.quantum_sobrante,sizeof(dtb.quantum_sobrante));
	offset+=sizeof(dtb.quantum_sobrante);

	//Flag de inicializacion
	memcpy(buffer+offset,&dtb.f_inicializacion,sizeof(dtb.f_inicializacion));
	offset+=sizeof(dtb.f_inicializacion);

	//Tamaño del script

	memcpy(buffer+offset,&tamanio_script,sizeof(tamanio_script));

	offset+=sizeof(tamanio_script);

	//Path del script

	memcpy(buffer+offset,dtb.escriptorio,tamanio_script);
	offset+=tamanio_script;

	//Cantidad de archivos abiertos por el DTB
	memcpy(buffer+offset,&cantidad_archivos,sizeof(cantidad_archivos));
	offset+=sizeof(cantidad_archivos);

	//Contenido de cada archivo abierto y su tamaño
	if(cantidad_archivos!=0){ //Hay archivos

		t_archivo* archivo;

		//Guardo el tamanio seguido del string de cada archivo
		for(i=0;i<cantidad_archivos;i++){
			archivo=list_get(dtb.archivos,i);

			int tamanio_archivo=string_length(archivo->path)+1;

			memcpy(buffer+offset,&tamanio_archivo,sizeof(int));
			offset+=sizeof(int);

			memcpy(buffer+offset,archivo->path,tamanio_archivo);
			offset+=tamanio_archivo;

			memcpy(buffer+offset,&(archivo->acceso),sizeof(int));
			offset+=sizeof(int);

		}

	}

	send(fd,buffer,tamanio_buffer,0);

	free(buffer);

}
/*
 * Funcion encargada de recibir y deserializar un DTB
*/
t_DTB RecibirYDeserializarDTB(int fd){

	void*buffer;
	t_DTB dtb;
	dtb.archivos=list_create();
	int offset=0;
	int tamanio_script,cant_archivos,i;
	int tamanio_buffer;

	//Recibo el header
	recv(fd,&tamanio_buffer,sizeof(tamanio_buffer),0);

	printf("tamanio buffer: %d\n",tamanio_buffer);

	buffer=malloc(tamanio_buffer);

	//Recibo el resto de la informacion
	recv(fd,buffer,tamanio_buffer,0);

	//Id del DTB
	memcpy(&(dtb.id),buffer+offset,sizeof(int));
	offset+=sizeof(int);

	//Program counter del DTB
	memcpy(&(dtb.pc),buffer+offset,sizeof(int));
	offset+=sizeof(int);

	//Quantum sobrante del DTB
	memcpy(&(dtb.quantum_sobrante),buffer+offset,sizeof(int));
	offset+=sizeof(int);

	//Flag de inicializacion
	memcpy(&(dtb.f_inicializacion),buffer+offset,sizeof(int));
	offset+=sizeof(int);

	//Tamaño del string del script
	memcpy(&tamanio_script,buffer+offset,sizeof(int));
	offset+=sizeof(int);

	//Ruta del script
	dtb.escriptorio=malloc(sizeof(char)*tamanio_script);
	memcpy(dtb.escriptorio,buffer+offset,tamanio_script);
	offset+=tamanio_script;

	//Cantidad de archivos
	memcpy(&cant_archivos,buffer+offset,sizeof(int));

	//Si hay archivos en el DTB...
	if(cant_archivos!=0){

		int tamanio_archivo;
		t_archivo* archivo=malloc(sizeof(t_archivo));

		offset+=sizeof(int);

		for(i=0;i<cant_archivos;i++){

			memcpy(&tamanio_archivo,buffer+offset,sizeof(int));
			offset+=sizeof(int);

			archivo->path=malloc(sizeof(char)*tamanio_archivo);

			memcpy(archivo->path,buffer+offset,tamanio_archivo);
			offset+=tamanio_archivo;

			memcpy(&(archivo->acceso),buffer+offset,sizeof(int));
			offset+=sizeof(int);

			list_add(dtb.archivos,archivo);

		}
	}

	return dtb;

}

int validarArchivoConfig(char *archivo){
	int f = open(archivo, O_RDONLY);
	if (f < 0){
		printf("no se encontro el archivo Configuracion %s.\n",archivo);
		return -1;
	}
	close(f);
	return 0;
}
