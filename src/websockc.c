#include <stdio.h>
#include <stdio.h>
#include <libwebsockets.h>

int ws_callback(struct lws *wsi, enum lws_callback reason, void *user, void *in, size_t len)
{
    ws_session_data *d = (ws_session_data*)user;
    
    switch(reason){
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            lwsl_err("CLIENT_CONNECTION_ERROR: %s\n", in ? (char*)in : "(null)");
            break;
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            lwsl_user("Connected to server\n");
            break;
        case LWS_CALLBACK_CLIENT_WRITEABLE:
            if (d->pending_send){
                int sent = lws_write(
                    wsi, 
                    &d->send_buf[LWS_PRE], 
                    data->send_len,
                    LWS_WRITE_TEXT
                );

                if (sent < (int)d->send_len){
                    lwsl_err("lws_write failed (%d)\n", n);
                    interrupted = 1;
                    return -1;
                }

                d->pending_send = 0;
                lwsl_user("Sent %d bytes.\n", n);
            }
            break;
        case LWS_CALLBACK_CLIENT_CLOSED:
            lwsl_user("Connection closed.\n");
            interrupted = 1;
            break;
        default:
            break;
    }

    return 0;
}

int make_proto_arr(
    struct lws_protocols **pa,
    char *proto_name, 
    int (*callback)(
        struct lws *wsi, 
        enum lws_callback_reasons reason, 
        void *user, 
        void *in, 
        size_t len
    ),
    int rx
)
{
    *pa = (struct lws_protocols*)calloc(2, sizeof(**pa));
    if ((*pa) == NULL){
        fprintf(stderr, "can't malloc protocols array for websocket creation\n");
        return -1;
    }

    strcpy(&(pa[0].name), proto_name);
    pa[0].callback = callback;
    pa[0].per_session_data_size = sizeof(ws_session_data);
    pa[0].rx_buffer_size = rx;
    pa[0].id = 0;
    pa[0].user = 0;
    pa[0].tx_packet_size = 0; //0 == same as rx_buffer_size

    pa[1] = { NULL, NULL, 0, 0, 0, NULL, 0 };
}

int make_lws_info(
	struct lws_context_creation_info **i, 
	struct lws_protocols *p,
)
{
	*i = (struct lws_context_creation_info*)calloc(1, sizeof(**i));
	if ((*i) == NULL){
		fprintf(stderr, "can't malloc info for context creation\n");
		return -1;
	}

    (*i)->port = CONTEXT_PORT_NO_LISTEN;
	(*i)->protocols = (*p);
	(*i)->gid = -1;
	(*i)->uid = -1;

	return 0;
}

int make_cc_info(
    struct lws_client_connect_info **cci,
    struct lws_context *c,
    char *h,
    struct lws_protocols *p
)
{
    int h_len = strlen(h) + 1;


    *cci = (struct lws_client_connect_info*)calloc(1, sizeof(**cci));
    (*cci)->context = c;

    (*cci)->address = (char*)calloc(h_len, sizeof(char));
    strcpy((&(*cci)->address), h);
    
    (*cci)->host = (char*)calloc(h_len, sizeof(char));
    strcpy((&(*cci)->host), host);
    
    (*cci)->origin = (char*)calloc(h_len, sizeof(char));
    strcpy((&(*cci)->origin), host);
    
    
}