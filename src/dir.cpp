#include <zmq.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>//rand
#include <time.h> //clock
#include <jansson.h>//json
#include <assert.h>
#include <map>
#include <string>
#include <iostream>
#include <algorithm> //for_each
#include <queue>
#include <random>


//-------------------data struct---------------------------
typedef struct 
{
    std::string id;
    double cpu_rate;
    double memory_total;
    double memory_used;
}delay_info_t;

//-------------------zmq---------------------------
void bind_local_address(void* socket, char *port) 
{
    char addr[32] = {0};
    char *addr_head = "tcp://*:";
    strcpy(addr, addr_head);
    strcat(addr, port);
    int rc = zmq_bind (socket, addr);
    printf("Bind dir: %s\n", addr);
    assert (rc == 0);
}

void send_to_one(void *socket, std::string socket_id, const char *msg)
{
    zmq_send(socket, socket_id.c_str(), 
             socket_id.length(), ZMQ_SNDMORE);
    zmq_send(socket, msg, 
             strlen(msg), 0);
}

void send_to_all(void *socket, std::queue<std::string> socket_id_list, char *msg) 
{
    while(!socket_id_list.empty())
    {
        std::string id = socket_id_list.front();
        send_to_one(socket, id.c_str(), msg);
        socket_id_list.pop();
    }
}

void send_request_to_delays(std::map<std::string, double> delay_weight_list, 
                            void *socket, char *msg, int &num) 
{
    for(std::map<std::string, double>::iterator it = delay_weight_list.begin();
       it != delay_weight_list.end(); ++it) 
    {
        //std::cout << it->first << "->" << it->second << std::endl;
        send_to_one(socket, it->first, msg);
        ++num;
    }
    sleep(1);
}

void recieve_from_one(void *socket, std::string &socket_id, 
                      std::string &message)
{
    char id[16] = {0};
    zmq_recv(socket, id, 16, 0);
    socket_id.assign(id);
    char msg[256] = {0};
    zmq_recv(socket, msg, 256, 0);
    message.assign(msg);
}

//-------------------json---------------------------
/**
*@brief: extract info from json string
*@param[out] - buf: system info 
*/
void json_to_string(char *buffer) 
{    
    json_error_t error;
    json_t *msg_rev;        
    const char *key;
    json_t *value;
    msg_rev = json_loadb(buffer, strlen(buffer), 0, &error);
    json_object_foreach(msg_rev, key, value) {
        printf("%s %s\n", key, json_string_value(value));
        //printf("%s %lf\n", key, json_number_value(value));
    }
}


int load_json_config_file(char *file_name, json_t *(&array)) 
{
    json_error_t error;
	array = json_load_file(file_name, JSON_DECODE_ANY, &error);
    if (!array) {
        perror("[ERROR] No client_file");
        return -1;
    }
    
    return 0;
}


void init_delay_list_with_config_file(std::map<std::string, double> &delay_list,
                                     char *file_path)
{
    json_t *info_array;
    load_json_config_file(file_path, info_array);
    json_t *info;
    int idx;
    json_array_foreach(info_array, idx, info) {
        
		json_t *name_obj = json_object_get(info, "name");
        const char *name = json_string_value(name_obj);
		//printf("client name: %s, len: %d\n", name, strlen(name));
        std::string name_str = std::string(name);
        delay_list[name_str] = 0;
    }          
}

void get_delay_info(delay_info_t &delay_info, std::string message)
{
    json_error_t error;
    json_t *info_obj = json_loadb(message.c_str(), message.length(), 
                                  0, &error);
    const char *key;
    json_t *value;
    json_object_foreach(info_obj, key, value) 
    {
        if(strcmp(key, "NickName") == 0)
            delay_info.id.assign(json_string_value(value));
        else if(strcmp(key, "cpu_rate") == 0)
            delay_info.cpu_rate = json_real_value(value);
        else if(strcmp(key, "mem_total") == 0)
            delay_info.memory_total = json_real_value(value);
        else if(strcmp(key, "mem_used") == 0)
            delay_info.memory_used = json_real_value(value);
        else
            fprintf(stderr, "[ERROR] Wrong Key word recieved: %s!\n", value);
    }
}

double get_delay_weight(delay_info_t delay_info)
{
    double cpu_rate = delay_info.cpu_rate;
    double mem_total = delay_info.memory_total;
    double mem_used = delay_info.memory_total;
    double weight = (cpu_rate + (mem_used / mem_total)) / 2;
    return weight;
}

void get_delay_weight_string_from_list(std::map<std::string, double> delay_weight_list, 
                                       std::string &message)
{
    
    std::map<std::string, double>::iterator it;
    json_t *delay_weight_array = json_array();
    for(it = delay_weight_list.begin(); it != delay_weight_list.end(); ++it)
    {
        int err;
        json_t *delay_weight = json_object();
        err = json_object_set(delay_weight, "NickName", json_string(it->first.c_str()));
        if(err == -1) 
        {
            fprintf(stderr, "[ERROR] Set NickName FAILED!\n");
        }
        err = json_object_set(delay_weight, "weight", json_real(it->second));
        if(err == -1) 
        {
            fprintf(stderr, "[ERROR] Set Weight FAILED!\n");
        }

        err = json_array_append_new(delay_weight_array, delay_weight);
        if(err == -1) 
        {
            fprintf(stderr, "[ERROR] Append Weight to array FAILED!\n");
        }
    }
    
    char msg[256] = {0};
    size_t size = json_dumpb(delay_weight_array, NULL, 0, 0);
    if (size == 0)
        return;
    size = json_dumpb(delay_weight_array, msg, size, 0);
    message.assign(msg);
    json_decref(delay_weight_array);
}

//------------------- helper ---------------------------
float time_difference(clock_t start, clock_t end)
{
    float diff_time = (float)(end - start) / CLOCKS_PER_SEC;
    return diff_time;
}

void dump_delay_list(std::map<std::string, int> m)
{
    std::map<std::string, int>::iterator it;
    for(it = m.begin(); it != m.end(); ++it) 
    {
        std::cout << "Id: " << it->first;
        std::cout << "-> Weight: " << it->second;
        std::cout << std::endl;
    }
}

void dump_delay_info_t(delay_info_t &info)
{
    std::cout << "[LOG] delay_info_t.id: " << info.id << std::endl;
    std::cout << "[LOG] delay_info_t.cpu_rate: " << info.cpu_rate << std::endl;
    std::cout << "[LOG] delay_info_t.mem_total: " << info.memory_total << std::endl;
    std::cout << "[LOG] delay_info_t.mem_used: " << info.memory_used << std::endl;
}

//-------------------main---------------------------
int main (int argc, char *argv[]) {
    srand((unsigned)time(NULL));
    
    if (argc != 4) {
        fprintf(stderr, "Usage: %s /name/ /port/to/delay /port/to/dir\n", argv[0]);
        return -1;
    }
    
    //--------------------------init the context------------------------------ 
    char *name = argv[1];
    char *port_to_delay = argv[2];
    char *port_to_dir = argv[3];
    
    //zmq init
    void *context = zmq_ctx_new ();
    void *router_to_dir = zmq_socket (context, ZMQ_ROUTER);
    void *router_to_delay = zmq_socket (context, ZMQ_ROUTER);
    zmq_setsockopt(router_to_delay, ZMQ_IDENTITY, name, strlen(name));
    zmq_setsockopt(router_to_dir, ZMQ_IDENTITY, "m", 1);
    
    //bind listen port
    bind_local_address(router_to_delay, port_to_delay);
    bind_local_address(router_to_dir, port_to_dir);
    zmq_pollitem_t router_item[] = {
        {router_to_delay, 0, ZMQ_POLLIN, 0},
        {router_to_dir, 0, ZMQ_POLLIN, 0}
    };
    sleep(2);
    printf("Dir server init ok!, my name: %s\n", name);
	
    //----------------load config file and init delay map---------------------- 
    //load json config file
    //delay info list stored nickname and weights---------------------
    std::map< std::string, double> delay_weight_list;
     
    init_delay_list_with_config_file(delay_weight_list, "./test.json");
    /*for(std::map<std::string, double>::iterator it = delay_weight_list.begin();
       it != delay_weight_list.end(); ++it) {
           std::cout << "node: " << it->first;
           std::cout << "-> weights: " << it->second;
           std::cout << std::endl;
    }*/
    int delay_nums;
    //collect delays' info when the server setup
    send_request_to_delays(delay_weight_list, router_to_delay, 
                           "hehe", delay_nums);

    //-----------------wait for reply--------------------------
    clock_t now_time = 0, last_time = 0;
    std::queue<std::string> dir_list;
    
    while (1) {
        //check wether over time, if over time, send request to
        //delays for collecting information
        now_time = clock();
        if (time_difference(last_time, now_time) > 0.05)
        {
            send_request_to_delays(delay_weight_list, router_to_delay, 
                                   "hehe", delay_nums);
            last_time = now_time;
        }
        
        //send weight info to dirs
        while(!dir_list.empty())
        {
            std::string id = dir_list.front();
            std::string message;
            get_delay_weight_string_from_list(delay_weight_list, message);
            send_to_one(router_to_dir, id.c_str(), message.c_str());
            dir_list.pop();
        }

        //reply from delays
        zmq_poll (router_item, 2, 10);
        if (router_item[0].revents & ZMQ_POLLIN) 
        {
            std::string socket_id;
            std::string message;
            
            recieve_from_one(router_to_delay, socket_id, message);
            delay_info_t info;
            get_delay_info(info, message);
            dump_delay_info_t(info);
            
            double weight = get_delay_weight(info);
            delay_weight_list[socket_id] = weight;
        }
        
        //recieve request from dir
        if (router_item[1].revents & ZMQ_POLLIN) 
        {
            std::string socket_id;
            std::string message;
            
            recieve_from_one(router_to_dir, socket_id, message);
            dir_list.push(socket_id);
        }

    }
    zmq_close(&router_to_dir);
    zmq_close(&router_to_delay);
    return 0;
}
