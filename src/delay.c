#include <stdio.h>
#include <zmq.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <jansson.h>//json
#include "get_system_info.h"

/**
*@brief: get system info and convert json object to string
*@param[out] - info: system info string 
*@param[out] - len: length of info 
*@reval - error:-1 success: 0
*/
int get_system_info(char *info, size_t *len, char *name) {
    //get system info
    double cpu_rate = get_cpu_rate();
    MEM_PACK *mem_info = get_memoccupy ();
    //printf("cpu rate: %lf, mem total: %lf, mem used: %lf\n", 
    //       cpu_rate, mem_info->total, mem_info->used_rate);

    //----------------json---------------------
    int err;
    json_t *json_reply = json_object();
    err = json_object_set(json_reply, "NickName", json_string(name));
    if (err == -1) {
        fprintf(stderr, "[ERROR] Set json object \"NickName\" FAILED!\n");
    }
    err = json_object_set(json_reply, "cpu_rate", json_real(cpu_rate));
    if (err == -1) {
        fprintf(stderr, "[ERROR] Set json object \"cpu_rate\" FAILED!\n");
    }
    err = json_object_set(json_reply, "mem_total", json_real(mem_info->total));
    if (err == -1) {
        fprintf(stderr, "[ERROR] Set json object \"mem_total\" FAILED!\n");
    }
    err = json_object_set(json_reply, "mem_used", json_real(mem_info->used_rate));
    if (err == -1) {
        fprintf(stderr, "[ERROR] Set json object \"mem_used\" FAILED!\n");
    }
	
    //json to string
    size_t size = json_dumpb(json_reply, NULL, 0, 0);
    if (size == 0)
        return -1;
	size = json_dumpb(json_reply, info, size, 0);
    json_decref(json_reply);
	*len = size;
    return 0;
}


int main (int argc, char *argv[]) {
    
    if (argc != 3) {
        fprintf(stderr, "Usage: %s /name/ /config/file/...\n", argv[0]);
        fprintf(stderr, "Now only name available!\n");
        return -1;
    }
    
    //-------------------zmq init-------------------
    char *name = argv[1];
    char *dir_file_path = argv[2];

    void *context = zmq_ctx_new ();
    void *router = zmq_socket(context, ZMQ_ROUTER);
    //char *dir_address = "tcp://localhost:5555";
    zmq_setsockopt(router, ZMQ_IDENTITY, name, strlen(name));
    
    json_error_t error;
    json_t *dir_array = json_load_file(dir_file_path, JSON_DECODE_ANY, &error);
    if (!dir_array) {
        perror("[ERROR] No client_file");
        return -1;
    }
    json_t *addr;
    int idx;
    json_array_foreach(dir_array, idx, addr) {
        const char *dir_addr = json_string_value(addr);
		printf("connect to : %s\n", dir_addr);
        zmq_connect(router, dir_addr);
    }          
    
    zmq_pollitem_t router_item = {router, 0, ZMQ_POLLIN, 0};
    printf("init done! connect to dir, my name is:%s, len is: %d\n", name, (int)strlen(name));

    while (1) {
        zmq_poll(&router_item, 1, -1);
        if (router_item.revents & ZMQ_POLLIN) {
			char id[16] = {0};
            zmq_recv(router, id, 16, 0);
            printf("id: %s len: %d\n", id, (int)strlen(id));
            char msg[256] = {0};
            zmq_recv(router, msg, 256, 0);
            printf("%s\n", msg);

            //get systeminfo
            char sysinfo[256] = {0};
            size_t len;
            get_system_info(sysinfo, &len, name);
            printf("sendinfo: %s len: %d\n", sysinfo, (int)len);
            zmq_send(router, id, strlen(id), ZMQ_SNDMORE);
            zmq_send(router, sysinfo, (int)len, 0);
            //zmq_send(router, " haodhfoashdf", 13, 0);

        }
    }
    
    zmq_close(router);
    zmq_ctx_destroy (context);
    return 0;
}
