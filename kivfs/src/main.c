/*----------------------------------------------------------------------------
 * main.c
 *
 *  Created on: 3. 10. 2013
 *      Author: Lukáš Kvídera, A11B0421P
 *	   Version: 0.0
 ---------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>

int init(){
	int res = 0;



}

void help(){
	printf("This is help for you");
}

int main(int argc, char **argv){

	if( argc <= 1){
		help();
	}

	if( !init(argc, argv) ){
		fprintf(stderr, "Initialisation failed!");
		return EXIT_FAILURE;
	}

	return fuse_main(argc, argv, &hello_oper, NULL);
}
