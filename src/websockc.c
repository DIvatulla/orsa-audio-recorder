#include <stdio.h>
#include <stdio.h>
#include <libwebsockets.h>
#include "../include/websockc.h"

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
    int rx_size)
{
    pl[0]->name = proto_name;
    pl[0]->callback = callback;
    pl[0]->per_session_data_size = sizeof(ws_session_data);
    pl[0]->callback = callback;
    pl[0]->per_session_data_size = sizeof(ws_session_data);
    pl[0]->rx_buffer_size = rx_size;
    pl[0]->id = 0;
    pl[0]->user = 0;
    pl[0]->tx_packet_size = 0; //0 == same as rx_buffer_size
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

    (*pl)[0]->name = (char*)calloc(strlen(proto_name)+1, sizeof(char));
    if ((*pl)[0]->name == NULL){
        fprintf(stderr, "can't malloc protocol name\n");
        free_proto_list(*pl);
        return -1;
    }

    init_proto_list(*pl, proto_name, callback, rx_size);
}

void free_proto_list_item(struct lws_protocols *p)
{
    if (p != NULL){
        free(p);
    }
}

void free_proto_list(ws_proto_list pl)
{
    if (pl != NULL){
        free_proto_list_item(pl[0]);
        free(pl);
    }
}

