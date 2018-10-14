#include "consola_MDJ.h"

void consola_MDJ(void){
	char * linea;
	char **linea_parseada;
	char* montaje;
	int nro_proceso;
	int status=1;
	do{
		montaje=string_duplicate(config_MDJ.mount_point);
		string_append(&montaje,"$");
		linea = readline(montaje);
		if(strlen(linea)!=0)
			add_history(linea);
		linea_parseada = parsearLinea(linea);
		if(*linea_parseada!=NULL){
			nro_proceso= identificarProceso(linea_parseada);
			ejecutarComando(nro_proceso,linea_parseada[1]);
			string_iterate_lines(linea_parseada,(void*)free);
			free(linea_parseada);
		}
		free(montaje);
		free(linea);
	}while(status);
}

char** parsearLinea(char* linea){
	char * espacio = " ";
	char** linea_parseada = string_split(linea,espacio);
	return linea_parseada;
}
int identificarProceso(char** linea_parseada){

	int i = 0,nro_op = 0,cantidad_operaciones = 5;

	char* operacion[cantidad_operaciones];

	operacion[0]="ls";
	operacion[1]="cd";
	operacion[2]="md5";
	operacion[3]="cat";
	operacion[4]="help";

	for(i=0;i<cantidad_operaciones;i++){
		if(strcmp(linea_parseada[0],operacion[i])==0){
			nro_op=i+1;
			return nro_op;
		}
	}

	return -1;
}

void ejecutarComando(int nro_op, char * args){
	switch(nro_op){
			case 1:
				printf("La operacion es ls y el parametro es: %s \n",args);
			break;
			case 2:
				printf("La operacion es cd y el parametro es: %s \n",args);
				string_append(&config_MDJ.mount_point,"/");
				string_append(&config_MDJ.mount_point,args);
			break;
			case 3:
				printf("La operacion es md5 y el parametro es: %s \n",args);
			break;
			case 4:
				printf("La operacion es cat y el parametro es: %s \n",args);
			break;
			case 5:
				printf("Operaciones disponibles:\n\tls [Para ver lo que hay en el directorio]\n\tcd <Directorio>\n\tmd5 <Archivo>\n\tcat <Archivo>\n");
			break;
			default:
				printf("No se reconoce la operacion, escriba 'help' para ver las operaciones disponibles\n");
	}
}
