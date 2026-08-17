#ifndef WEBSOCKC_H
#define WEBSOCKC_H

#include <libwebsockets.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#ifndef MAX_PAYLOAD
#define MAX_PAYLOAD 2621440 //5mb
#endif

#define WS_DEFAULT_PORT 80

typedef struct{
    unsigned char *buf; //LWS_PRE bytes of headroom for framing + MAX_PAYLOAD
    size_t size;
    int wi;
} ws_session_data;
typedef struct lws_protocols **ws_proto_list;

typedef enum{
    WS_NONE,
    WS_START,
    WS_SEND,
    WS_SEND_END,
    WS_RECV,
    WS_RECV_END,
    WS_KILL,
    WS_ERR
} ws_state;

typedef struct ws_queue_item{
    unsigned char *data;
    int size;
    struct ws_queue_item *next;
} ws_queue_item;

typedef struct{
    ws_queue_item *head;
	ws_queue_item *tail;
    int item_count;
    int res_size;
} ws_queue;


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
    int rx_size);

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
    int rx_size);

void free_proto_list(ws_proto_list pl);

int make_context_creation_info(
    struct lws_context_creation_info **info,
    ws_proto_list pl);

void free_context_creation_info(struct lws_context_creation_info *info);

int make_client_connection_info(
    struct lws_client_connect_info **cc_info,
    struct lws_context *context,
    ws_proto_list pl,
    char *address,
    int port,
    char *path,
    char *host_header,
    char *origin_header);

void free_client_connection_info(struct lws_client_connect_info *cc_info);

int make_ws_queue_item(ws_queue_item **wsqi, unsigned char *data, int data_size);
void free_ws_queue_item(ws_queue_item *wsqi);
int make_ws_queue(ws_queue **wsq);
int push_ws_queue(ws_queue *wsq, ws_queue_item **item);
int pop_ws_queue(ws_queue *wsq, ws_queue_item **item);
void free_ws_queue(ws_queue *wsq);


#endif