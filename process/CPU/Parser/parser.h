/*
 * parser.h
 *
 *  Created on: 6 sep. 2018
 *      Author: utnso
 */

#ifndef PROCESS_S_AFA_PARSER_H_
#define PROCESS_S_AFA_PARSER_H_

#define OP_ABRIR "abrir"
#define OP_CONCENTRAR "concentrar"
#define OP_ASIGNAR "asignar"
#define OP_WAIT "wait"
#define OP_SIGNAL "signal"
#define OP_FLUSH "flush"
#define OP_CLOSE "close"
#define OP_CREAR "crear"
#define OP_BORRAR "borrar"
#define COMENTARIO '#'

typedef struct{

	enum{
		ABRIR,
		CONCENTRAR,
		ASIGNAR,
		WAIT,
		SIGNAL,
		FLUSH,
		CLOSE,
		CREAR,
		BORRAR,
		COMENT
	}keyword;
	union{
		struct{
			char* path;
		}abrir;

		struct{
			char*path;
			int linea;
			char*valor;
		}asignar;

		struct{
			char*recurso;
		}wait;

		struct{
			char*recurso;
		}signal;

		struct{
			char*path;
		}flush;

		struct{
			char*path;
		}close;

		struct{
			char*path;
			int lineas;
		}crear;

		struct{
			char*path;
		}borrar;

	}argumentos;

	char** _free;

} t_operacion;

void destroyParse(t_operacion );
t_operacion parseLine(char* );

#endif /* PROCESS_S_AFA_PARSER_H_ */
