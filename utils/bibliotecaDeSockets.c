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
