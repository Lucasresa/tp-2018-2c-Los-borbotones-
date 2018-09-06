#include "gestor-g.dt.h"

int main(){
	consola();
	return 0;
}

void consola(){
	char * linea;
	char **linea_parseada;
	int nro_proceso;
	int status;
	do{
		linea = readline("$");
		linea_parseada = parsearLinea(linea);
		if(*linea_parseada!=NULL){
			nro_proceso= identificarProceso(linea_parseada);
			ejecutarProceso(nro_proceso,linea_parseada[1]);
			free(linea_parseada);
		}

		free(linea);
	}while(status);
}

char** parsearLinea(char* linea){
	char * espacio = " ";
	char** linea_parseada = string_split(linea,espacio);
	return linea_parseada;
}
int identificarProceso(char** linea_parseada){

	int i = 0,nro_op = 0,cantidad_operaciones = 4;

	char* operacion[cantidad_operaciones];

	operacion[0]="ejecutar";
	operacion[1]="status";
	operacion[2]="finalizar";
	operacion[3]="metricas";

	for(i=0;i<cantidad_operaciones;i++){
		if(strcmp(linea_parseada[0],operacion[i])==0){
			nro_op=i+1;
			return nro_op;
		}
	}
	return -1;
}

void ejecutarProceso(int nro_op, char * args){
	switch(nro_op){
			case 1:
				printf("La operacion es ejecutar y el parametro es: %s \n",args);
			break;
			case 2:
				printf("La operacion es status y el parametro es: %s \n",args);
			break;
			case 3:
				printf("La operacion es finalizar y el parametro es: %s \n",args);
				exit(4);
			break;
			case 4:
				printf("La operacion es metricas y el parametro es: %s \n",args);
			break;
			default:
				printf("No se reconoce la operacion \n");
	}
}
