#include <stdio.h>
#include <stdlib.h>
#include "parser.h"
#include <commons/string.h>

//Funcion que abre un archivo solo para lectura
FILE* openFile(char* path){

	FILE* fd=fopen(path,"r");

	if(!fd){
		perror("El archivo no se pudo abrir");
		return NULL;
	}

	return fd;

}

//Funcion para liberar la memoria usada para parsear las lineas
void destroyParse(t_operacion op){
	if(op._free){
		string_iterate_lines(op._free,(void*)free);
		free(op._free);
	}
}


//Funcion encargarda de ir parseando las lineas
t_operacion parseLine(char* line){

	t_operacion op;

	char* auxLine=string_duplicate(line);

	string_trim(&auxLine);

	if(auxLine[0]==COMENTARIO){
		op.keyword=comentario;
		return op;
	}

	char**split = string_split(auxLine," ");

	op._free=split;

	//operacion a realizar
	char* keyword=split[0];

	if(string_equals_ignore_case(keyword,OP_ABRIR)){
		op.keyword=abrir;
		op.argumentos.abrir.path=split[1];
	}
	else if(string_equals_ignore_case(keyword,OP_CONCENTRAR)){
		op.keyword=concentrar;
	}
	else if(string_equals_ignore_case(keyword,OP_ASIGNAR)){
		op.keyword=asignar;
		op.argumentos.asignar.path=split[1];
		op.argumentos.asignar.linea=(int)strtol(split[2],(char**)NULL,10);
		op.argumentos.asignar.valor=split[3];
	}
	else if(string_equals_ignore_case(keyword,OP_WAIT)){
		op.keyword=wait;
		op.argumentos.wait.recurso=split[1];
	}
	else if(string_equals_ignore_case(keyword,OP_SIGNAL)){
		op.keyword=signal;
		op.argumentos.signal.recurso=split[1];
	}
	else if(string_equals_ignore_case(keyword,OP_FLUSH)){
		op.keyword=flush;
		op.argumentos.flush.path=split[1];
	}
	else if(string_equals_ignore_case(keyword,OP_CLOSE)){
		op.keyword=close;
		op.argumentos.close.path=split[1];
	}
	else if(string_equals_ignore_case(keyword,OP_CREAR)){
		op.keyword=crear;
		op.argumentos.crear.path=split[1];
		op.argumentos.crear.lineas=(int)strtol(split[2],(char**)NULL,10);
	}
	else if(string_equals_ignore_case(keyword,OP_BORRAR)){
		op.keyword=borrar;
		op.argumentos.borrar.path=split[1];
	}
	free(auxLine);
	return op;
}

//Main para testear el funcionamiento del parser, se debe quitar al momento de usarlo en los procesos que requieran parser
int main(){

	FILE* fd=openFile("script.txt");
	t_operacion op;
	size_t len=0;
	ssize_t read;
	char*line=NULL;


	while((read=getline(&line,&len,fd))!=-1){

		op=parseLine(line);

		switch(op.keyword){

		case abrir:
			printf("abrir\tpath: %s\n",op.argumentos.abrir.path);
			break;
		case concentrar:
			printf("concentrando....\n");
			break;
		case asignar:
			printf("asignar\tpath: %s\tlinea: %d\tvalor: %s\n",op.argumentos.asignar.path,op.argumentos.asignar.linea,
					op.argumentos.asignar.valor);
			break;
		case close:
			printf("close\tpath: %s\n",op.argumentos.close.path);
			break;
		case crear:
			printf("crear\tpath: %s\tlineas: %d\n",op.argumentos.crear.path,op.argumentos.crear.lineas);
			break;
		case borrar:
			printf("borrar\tpath: %s\n",op.argumentos.borrar.path);
			break;
		case comentario:
			printf("Linea comentada, salteando....\n");
			break;
		default:
			printf("Linea invalida, cerrando...\n");
			free(op._free);
			exit(1);

		}

		destroyParse(op);//Libero memoria
	}

	free(line);

	return 1;



}



