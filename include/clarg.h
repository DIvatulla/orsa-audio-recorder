#ifndef CLARG_H
#define CLARG_H

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
	INPUT_DEV = 0,
	OUTPUT_DEV = 1,
	WS_HOST = 2,
    WS_PORT = 3,
    WS_PATH = 4,
    WS_CERT = 5,
    CLARG_COUNT = 6
} clarg_enum;

void free_clargs(char **clargs);
int copy_clarg(char *arg, char **var);
char **parse_clargs(int argc, char **argv);


#endif