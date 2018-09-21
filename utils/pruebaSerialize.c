#include "bibliotecaDeSockets.h"

int main(){

	void*buffer;

	int tamanio_buffer;

	t_DTB dtb={.id=0,.pc=0,.f_inicializacion=0,.escriptorio="holamundo/pepe.txt",.cant_archivos=3};

	dtb.archivos=malloc(dtb.cant_archivos);

	dtb.archivos[0]="archivo0";
	dtb.archivos[1]="archivo1";
	dtb.archivos[2]="archivo2";

	printf("Empezando la serializacion...\n");

	serializarDTB(&buffer,dtb);

	printf("Serializacion finalizada....\n");

	memcpy(&tamanio_buffer,buffer,sizeof(int));

	printf("Tamanio del buffer: %d\n",tamanio_buffer);

	void*buffer2=malloc(tamanio_buffer);

	t_DTB dtb_deserializado;

	printf("Deserializando DTB.....\n");

	deserializarDTB(buffer2,&dtb_deserializado);

	printf("Deserializacion finalizada!\n");

}

