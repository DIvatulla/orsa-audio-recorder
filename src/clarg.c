#include "../include/clarg.h"
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

int copy_clarg(char *arg, char **var)
{
    *var = (char*)calloc((strlen(arg)+1), sizeof(char));
    strcpy(*var, arg);
}

char **parse_clargs(int argc, char **argv)
{
	int opt;
    char **res = (char**)calloc(CLARG_COUNT, sizeof(char*));

	while ((opt = getopt(argc, argv, ":i:o:h:p:w:c:")) != -1){
		switch (opt){
		case 'i':
			printf("Option i has arg: %s\n", optarg);
            copy_clarg(optarg, &res[INPUT_DEV]);
			break;
		case 'o':
			printf("Option o has arg: %s\n", optarg);
            copy_clarg(optarg, &res[OUTPUT_DEV]);
			break;
		case 'h':
			printf("Option h has arg: %s\n", optarg);
            copy_clarg(optarg, &res[WS_HOST]);
			break;
		case 'p':
			printf("Option p has arg: %s\n", optarg);
            copy_clarg(optarg, &res[WS_PORT]);
			break;
        case 'w':
            printf("Option w has arg: %s\n", optarg);
            copy_clarg(optarg, &res[WS_PATH]);
            break;
        case 'c':
            copy_clarg(optarg, &res[WS_CERT]);
            break;
		case '?':
			fprintf(stderr, "Unknown option: %c\n", optopt);
            opt = ':';
		case ':':
			fprintf(stderr, "Missing arg for %c\n", optopt);
			free_clargs(res);
            res = NULL;
            goto end;
            break;  
		}
	}

end:
    return res;
}

void free_clargs(char **clargs)
{
    int i = 0;

    for (i = 0; i < CLARG_COUNT; i++){
        if (clargs[i]){
            free(clargs[i]);
        }
    }

    free(clargs);
}