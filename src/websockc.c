#include <stdio.h>
#include <stdio.h>
#include <libwebsockets.h>
#include "../include/websockc.h"

#define BLOCK_SIGNAL(SIGNAL)\
    do{\
        sigset_t set;\
	    sigemptyset(&set);\
        sigaddset(&set, SIGNAL);\
	    pthread_sigmask(SIG_BLOCK, &set, NULL);\
    } while(0)


int malloc_str_fields(int amount, ...)
{
    int i, err, field_size;
    char **field = NULL;
    va_list list;

    va_start(list, amount);
    for (i = err = 0; i < amount; i++){
        field = va_arg(list, char**);
        field_size = va_arg(list, int);

        *field = (char*)calloc(field_size, sizeof(char));
        if ((*field) == NULL){
            va_end(list);
            return -1;
        }
    }
    va_end(list);

    return 0;
}

void free_str_fields(int amount, ...)
{
    int i = 0;
    char **field = NULL;
    va_list list;

    va_start(list, amount);
    for (i = 0; i < amount; i++){
        field = va_arg(list, char**);
        printf("another field in struct to be freed - %s\n", *field);
        if ((*field) != NULL){
            free(*field);
        }
    }
    va_end(list);
}

/*
item 0 of protocol list will carry direct pointer to stack allocated 
constant in main file with ws protocol name
*/
void init_proto_list(
    ws_proto_list pl,
    char *proto_name, 
    int (*callback)(
        struct lws *wsi, 
        enum lws_callback_reasons reason, 
        void *user, 
        void *in, 
        size_t len
    ),
    int per_session_data_size,
    int rx_size)
{
    pl[0]->name = proto_name;
    pl[0]->callback = callback;
    pl[0]->per_session_data_size = per_session_data_size;
    pl[0]->rx_buffer_size = rx_size;
    pl[0]->id = 0;
    pl[0]->user = 0;
    pl[0]->tx_packet_size = 0; //0 means same as rx_buffer_size
}

int make_proto_list(
    ws_proto_list *pl,
    char *proto_name, 
    int (*callback)(
        struct lws *wsi, 
        enum lws_callback_reasons reason, 
        void *user, 
        void *in, 
        size_t len
    ),
    int per_session_data_size,
    int rx_size)
{ 
    *pl = (ws_proto_list)calloc(2, sizeof(struct lws_protocols*));
    if ((*pl) == NULL){
        fprintf(stderr, "can't malloc protocols list for websocket creation\n");
        return -1;
    }

    (*pl)[0] = (struct lws_protocols*)calloc(1, sizeof(struct lws_protocols));
    if (((*pl)[0]) == NULL){
        fprintf(stderr, "can't malloc protocols list's first element\n");
        free_proto_list(*pl);
        return -1;
    }

    init_proto_list(*pl, proto_name, callback, per_session_data_size, rx_size);

    return 0;
}

void free_proto_list(ws_proto_list pl)
{
    if (pl != NULL){
        if (pl[0] != NULL){
            free(pl[0]);
        }
        free(pl);
    }
}

int make_context_creation_info(
    struct lws_context_creation_info **info,
    ws_proto_list pl)
{
    *info = (struct lws_context_creation_info*)calloc(
        1, 
        sizeof(struct lws_context_creation_info)
    );
    if ((*info) == NULL){
        fprintf(stderr, "can't malloc lws context creation info\n");
        return -1;
    }
    
    (*info)->port = CONTEXT_PORT_NO_LISTEN;
    (*info)->pprotocols = (const struct lws_protocols**)pl;
    (*info)->gid = -1;
    (*info)->uid = -1;

    return 0;
}

void free_context_creation_info(struct lws_context_creation_info *info)
{
    free(info);
}

void init_client_connection_info(
    struct lws_client_connect_info *cc_info,
    struct lws_context *context,
    ws_proto_list pl,
    char *address,
    int port,
    char *path,
    char *host_header,
    char *origin_header)
{
    cc_info->context = context;
    cc_info->port = port;
	cc_info->ssl_connection = 0;
    strcpy((char*)cc_info->address, address);
    strcpy((char*)cc_info->path, path);
    strcpy((char*)cc_info->host, host_header);
    strcpy((char*)cc_info->origin, origin_header);
	strcpy((char*)cc_info->protocol, pl[0]->name);
}

int make_client_connection_info(
    struct lws_client_connect_info **cc_info,
    struct lws_context *context,
    ws_proto_list pl,
    char *address,
    int port,
    char *path,
    char *host_header,
    char *origin_header)
{
    int err = 0;

    *cc_info = (struct lws_client_connect_info*)calloc(
        1, 
        sizeof(struct lws_client_connect_info)
    );
    if ((*cc_info) == NULL){
        fprintf(stderr, "can't malloc lws client connect info\n");
        return -1;
    }

    err = malloc_str_fields(5, 
        &(*cc_info)->address, strlen(address) + 1,
        &(*cc_info)->path, strlen(path) + 1,
        &(*cc_info)->host, strlen(host_header) + 1,
        &(*cc_info)->origin, strlen(origin_header) + 1,
        &(*cc_info)->protocol, strlen(pl[0]->name) + 1
    );
    if (err < 0){
        fprintf(stderr, "can't malloc string fields of client connect info\n");
        free_client_connection_info(*cc_info);
    }

    init_client_connection_info(*cc_info, context, pl, address,
        port, path, host_header, origin_header);

    return 0;
}

void free_client_connection_info(struct lws_client_connect_info *cc_info)
{
    free_str_fields(5, &cc_info->address, &cc_info->path, &cc_info->host,
        &cc_info->origin, &cc_info->protocol);
    free(cc_info);
}

int make_ws_queue_item(ws_queue_item **wsqi, unsigned char *data, int data_size) //creates new queue item ponting to argument
{
    *wsqi = (ws_queue_item*)calloc(1, sizeof(ws_queue_item));
    if (!(*wsqi)){
        return -1;
    }
    (*wsqi)->data = (unsigned char*)calloc(data_size, sizeof(char));
    if (!((*wsqi)->data)){
        return -1;
    }

    (*wsqi)->data = data;
    (*wsqi)->size = data_size;

    return 0;
}

void free_ws_queue_item(ws_queue_item *wsqi)
{
    wsqi->data ? free(wsqi->data) : 0;
    free(wsqi);
}

int make_ws_queue(ws_queue **wsq)
{
    *wsq = (ws_queue*)calloc(1, sizeof(ws_queue));
    if (!(*wsq)){
        return -1;
    }
    (*wsq)->head = NULL;
    (*wsq)->tail = (*wsq)->head;

    pthread_mutex_init(&((*wsq)->mutex), NULL);
    pthread_cond_init(&((*wsq)->cond), NULL);

    return 0;
}

int push_ws_queue(ws_queue *wsq, ws_queue_item **item)
{
	if (!wsq->head){
		wsq->head = wsq->tail = *item;
	}
	else{
		wsq->tail->next = *item;
		wsq->tail = *item;
	}
	wsq->tail->next = NULL;
    return 0;
}

int pop_ws_queue(ws_queue *wsq, ws_queue_item **item)
{
    if (!wsq->head){
        printf("Empty queue\n");
        return -1;
    }
    if (*item){
        printf("Var to pop from queue is not empty\n");
        return -1;
    }

    *item = wsq->head;
    wsq->head = wsq->head->next;

    return 0;
}

void free_ws_queue(ws_queue *wsq)
{
    ws_queue_item *tmp;

    if (wsq->head){
        while(wsq->head){
            tmp = wsq->head;
            wsq->head = wsq->head->next;
            
            if (tmp->data){
                free(tmp->data);
            } 
            free(tmp);
        }
    }
    free(wsq);
}