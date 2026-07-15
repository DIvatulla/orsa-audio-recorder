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

#endif