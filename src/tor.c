#include <zmq.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "jansson.h"

#include "container.h"
#include "util.h"

typedef struct 
{
    char NickName[128];
    double weight;
}delay_t;

int compare_delay_search(const void *delay, const void **member)
{
    const delay_t *dly = delay;
    const delay_t *mem = *member;
    return strcmp(dly->NickName, mem->NickName);
}

void get_delay_weight_from_json(smartlist_t *sl, char *message, int len)
{
    json_error_t error;
    json_t *delay_weight_list = json_loadb(message, len, 0, &error);
    
    int idx;
    json_t *delay_weight;
    json_array_foreach(delay_weight_list, idx, delay_weight)
    {
        delay_t *delay = (delay_t *)tor_calloc(1, sizeof(delay_t));//may memory leak!!!!!
        json_t *nick_name = json_object_get(delay_weight, "NickName");
        json_t *weight = json_object_get(delay_weight, "weight");
        strcpy(delay->NickName, json_string_value(nick_name));
        delay->weight = json_real_value(weight);

        smartlist_add(sl, delay);
    }
    //smartlist_sort(sl, compare_delay_sort);
}

int main (int argc, char *argv[]) {
    
    if (argc !=2) {
        fprintf(stderr, "Usage: %s port\n", argv[0]);
        return -1;
    }
    void *context = zmq_ctx_new ();


    //  Socket to talk to server
    char addr[256];
    char *addr_head = "tcp://localhost:";
    char* port = argv[1];
    memset(addr, 0, 256);
    strcpy(addr, addr_head);
    strcat(addr, port);
    void *requester = zmq_socket (context, ZMQ_ROUTER);
    zmq_setsockopt(requester, ZMQ_IDENTITY, "my", 2);
    zmq_connect (requester, addr);
    printf("connect to %s\n", addr);
    sleep(1);//important
    
    int size = zmq_send (requester, "m", 1, ZMQ_SNDMORE);
    printf("send message to %d, %s\n", size, addr);
    size = zmq_send (requester, "hello", 5, 0);
    printf("send message to %d, %s\n", size, addr);
    char buffer [256] = {0};
    size = zmq_recv (requester, buffer, 255, 0);
    size = zmq_recv (requester, buffer, 255, 0);
    if (size == -1) {
        printf("dont recieve message\n");
        return -1;
    }
    buffer[size] = '\0';
	//printf("%s\n", buffer);
   
    //tor
    smartlist_t *sl = smartlist_new();
    get_delay_weight_from_json(sl, buffer, size);
    
    //foreach every delay
    SMARTLIST_FOREACH_BEGIN(sl, delay_t *, delay)
    {
        printf("nickname: %s, weight: %lf\n", delay->NickName, delay->weight);

    } SMARTLIST_FOREACH_END(delay);
    
    //find the delay
    delay_t *dl = smartlist_bsearch(sl, "A", compare_delay_search);
    //printf("Nickname: %s, weight: %lf\n", dl->NickName, dl->weight);
    smartlist_free(sl);//may memory leak
    zmq_close (requester);
    zmq_ctx_destroy (context);
    
    
    return 0;
}
