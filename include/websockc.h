#ifndef WEBSOCKC_H
#define WEBSOCKC_H

#include <libwebsockets.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#ifndef MAX_PAYLOAD
#define MAX_PAYLOAD 5242880 //5mb
#endif

#define WS_DEFAULT_PORT 80

typedef struct{
    unsigned char send_buf[LWS_PRE + MAX_PAYLOAD]; /* LWS_PRE bytes of headroom for framing */
    size_t send_len;
    int    pending_send;
} ws_session_data;
typedef struct lws_protocols **ws_proto_list;

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

#endif