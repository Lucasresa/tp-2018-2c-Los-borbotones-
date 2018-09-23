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
	case PAQ_INT:
		serializarYEnviarEntero(socket, (int*)package);
		break;
	case PAQ_STRING:
		serializarYEnviarString(socket, (char*)package);
		break;
	case PAQ_PCB:
		serializarYEnviarEntero(socket, &((struct mProc*)package)->PC);
		serializarYEnviarEntero(socket, &((struct mProc*)package)->PID);
		serializarYEnviarEntero(socket, &((struct mProc*)package)->estadoDelProceso);
		serializarYEnviarString(socket, ((struct mProc*)package)->rutaRelativaDeArchivo);
		break;
	case PAQ_MPROCESO:
		serializarYEnviarEntero(socket,&((struct mProcesoSwap*)package)->id);
		serializarYEnviarEntero(socket,&((struct mProcesoSwap*)package)->cantidadTotalPaginas);
		break;
	case PAQ_PETICION_PAGINA_SWAP:
		serializarYEnviarEntero(socket,&((struct peticionLecturaSwap*)package)->idProceso);
		serializarYEnviarEntero(socket,&((struct peticionLecturaSwap*)package)->numeroPagina);
		break;
	case PAQ_LECTURA_CONTENIDO:
		serializarYEnviarString(socket,((struct paqContenido*)package)->contenido);
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

void* recibirYDeserializar(int socket){

	int *tipo = (int*)recibirYDeserializarEntero(socket);

	if(tipo ==NULL){
		return NULL;
	}
	switch(*tipo){
	case PAQ_INT:
		return (void*)recibirYDeserializarEntero(socket);

	case PAQ_STRING:
		return recibirYDeserializarString(socket);

	case PAQ_PCB:
	{
		struct mProc* pcb = malloc(sizeof(struct mProc));
		pcb->PC = *recibirYDeserializarEntero(socket);
		pcb->PID = *recibirYDeserializarEntero(socket);
		pcb->estadoDelProceso = *recibirYDeserializarEntero(socket);
		pcb->rutaRelativaDeArchivo = recibirYDeserializarString(socket);

		return pcb;
	}
	case PAQ_MPROCESO:
	{
		struct mProcesoSwap* proceso = malloc(sizeof(struct mProcesoSwap));
		proceso->id = *recibirYDeserializarEntero(socket);
		proceso->cantidadTotalPaginas = *recibirYDeserializarEntero(socket);

		return proceso;
	}
	case PAQ_LECTURA_CONTENIDO:
	{
		struct paqContenido* contenido = malloc(sizeof(struct paqContenido));
		contenido->contenido = recibirYDeserializarString(socket);

		return contenido;
	}
	case PAQ_PETICION_PAGINA_SWAP:
	{
		struct peticionLecturaSwap* peticion = malloc(sizeof(struct peticionLecturaSwap));
		peticion->idProceso = *(int*)recibirYDeserializar(socket);
		peticion->numeroPagina = *(int*)recibirYDeserializar(socket);

		return peticion;
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

//-----------------------------------------------------------------------
/*Funcion para serializar el DTB y poder enviarlo a traves de un socket
 *Se copia todo lo que hay dentro del DTB a un buffer, la estructura seria:
 *
 *	|Tamañobuffer|Id|Pc|Flag|TamañoScript|Script|CantidadArchivosAbiertos|TamanioArchivo1|Archivo1|...|TamanioArchivoN|ArchivoN|
 *
 *Lo que retorna es el tamaño total del buffer a enviar
*/
void serializarYEnviarDTB(int fd,void* buffer,t_DTB dtb){

	int i, tamanio_info;
	int offset=0;
	int tamanio_script=strlen(dtb.escriptorio)+1;

	//Calculo el tamaño total que tendra el buffer y reservo memoria para el mismo
	int tamanio_buffer=(sizeof(int)*6)+(sizeof(char)*tamanio_script);

	//Reservo memoria para cada archivo abierto si es que lo hay
	if(dtb.cant_archivos!=0){

		for(i=0;i<dtb.cant_archivos;i++){

			tamanio_buffer+=((sizeof(char)*strlen(dtb.archivos[i]))+1);

		}

		tamanio_buffer+=sizeof(int)*dtb.cant_archivos;

	}

	tamanio_info=tamanio_buffer-sizeof(tamanio_buffer);

	buffer=malloc(tamanio_buffer);

	memset(buffer,0,tamanio_buffer);

	printf("Tamanio buffer= %d\tTamanio info= %d\n",tamanio_buffer,tamanio_info);

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
	//Flag de inicializacion

	memcpy(buffer+offset,&dtb.f_inicializacion,sizeof(dtb.f_inicializacion));
	offset+=sizeof(dtb.f_inicializacion);

	//Tamaño del script

	memcpy(buffer+offset,&tamanio_script,sizeof(tamanio_script));

	offset+=sizeof(tamanio_script);

	//Path del script

	memcpy(buffer+offset,dtb.escriptorio,tamanio_script);
	offset+=tamanio_script;

	memcpy(buffer+offset,&dtb.cant_archivos,sizeof(dtb.cant_archivos));

	offset+=sizeof(dtb.cant_archivos);

	//Cantidad de archivos en el array de string
	if(dtb.cant_archivos!=0){ //Hay archivos

		//Guardo el tamanio seguido del string de cada archivo
		for(i=0;i<dtb.cant_archivos;i++){
			int tamanio_archivo=string_length(dtb.archivos[i])+1;

			memcpy(buffer+offset,&tamanio_archivo,sizeof(int));
			offset+=sizeof(int);

			memcpy(buffer+offset,dtb.archivos[i],tamanio_archivo);
			offset+=tamanio_archivo;
		}

	}

	send(fd,buffer,tamanio_buffer,0);

	free(buffer);

}
/*
	Funcion para deserializar el buffer.
	1- Recibir el tamaño del buffer (que es un int) y con eso reservar memoria en un void* para recibir el resto
	2- Recibir el resto del buffer (con otro recv)
	3- Invocar a la funcion deserializar con sus respectivos parametros
	4- El contenido deserializado se guardara en el DTB que se pasa como parametro
*/
t_DTB RecibirYDeserializarDTB(int fd){

	void*buffer;
	t_DTB dtb;
	int offset=0;
	int tamanio_script,cant_archivos,i;
	int tamanio_buffer;

	//Recibo el header
	recv(fd,&tamanio_buffer,sizeof(tamanio_buffer),0);

	printf("tamanio buffer: %d\n",tamanio_buffer);

	buffer=malloc(tamanio_buffer);

	recv(fd,buffer,tamanio_buffer,0);

	//Id del DTB

	memcpy(&(dtb.id),buffer+offset,sizeof(int));
	offset+=sizeof(int);

	printf("check! %d\n",dtb.id);

	//Program counter del DTB

	memcpy(&(dtb.pc),buffer+offset,sizeof(int));
	offset+=sizeof(int);

	printf("check!\n");

	//Flag de inicializacion

	memcpy(&(dtb.f_inicializacion),buffer+offset,sizeof(int));
	offset+=sizeof(int);

	printf("check!\n");


	//Tamaño del string del script
	memcpy(&tamanio_script,buffer+offset,sizeof(int));
	offset+=sizeof(int);

	printf("check!\n");


	//Ruta del script

	dtb.escriptorio=malloc(sizeof(char)*tamanio_script);
	memcpy(dtb.escriptorio,buffer+offset,tamanio_script);
	offset+=tamanio_script;

	printf("check!\n");


	//Cantidad de archivos

	memcpy(&cant_archivos,buffer+offset,sizeof(int));

	printf("check! %d\n",cant_archivos);


	//Si hay archivos en el DTB...
	if(cant_archivos!=0){

		int tamanio_archivo;

		dtb.archivos=malloc(cant_archivos);
		dtb.cant_archivos=cant_archivos;

		offset+=sizeof(int);

		for(i=0;i<cant_archivos;i++){

			memcpy(&tamanio_archivo,buffer+offset,sizeof(int));
			offset+=sizeof(int);

			dtb.archivos[i]=malloc(sizeof(char)*tamanio_archivo);

			memcpy(dtb.archivos[i],buffer+offset,tamanio_archivo);
			offset+=tamanio_archivo;

		}
	}else{

		printf("agrego cant. de archivos\n");

		dtb.cant_archivos=cant_archivos;

	}
	return dtb;
}
