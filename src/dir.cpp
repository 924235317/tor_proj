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
//delay info struct
typedef struct 
{
    std::string id;
    double cpu_rate;
    double memory_total;
    double memory_used;
}delay_info_t;

//-----------------------zmq-------------------------------
/**
*@brief: bind socket to local port
*@param[in] - socket: socket to bind
*@param[in] - port: local port 
**/
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

/**
*@brief: send message to one zmq routers
*@param[in] - socket: socekt
*@param[in] - socket_id: router id to send information
*@param[out] - msg: informationg to be sent
**/
void send_to_one(void *socket, std::string socket_id, const char *msg)
{
    zmq_send(socket, socket_id.c_str(), 
             socket_id.length(), ZMQ_SNDMORE);
    zmq_send(socket, msg, 
             strlen(msg), 0);
}

/**
*@brief: send message to all zmq routers in list
*@param[in] - socket: socekt
*@param[in] - socket_id_list: router id list to send information
*@param[out] - msg: information to be sent
**/
void send_to_all(void *socket, std::queue<std::string> socket_id_list, char *msg) 
{
    while(!socket_id_list.empty())
    {
        std::string id = socket_id_list.front();
        send_to_one(socket, id.c_str(), msg);
        socket_id_list.pop();
    }
}

/**
*@brief: send request to all delays in the weight list
*@param[in] - delay_weight_list: dictionary contains delays' id and weights
*@param[in] - socket: socekt
*@param[out] - msg: information to be sent
*@param[out] - num: number of delays in the list
**/
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

/**
*@brief: recieve message from one router
*@param[in] - socket: socekt
*@param[out] - socket_id: socket id that recieved message
*@param[out] - message: recieved information 
**/
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
*@brief: extract info from json to string
*@param[out] - buffer: system info 
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

/**
*@brief: load configure file, may be dir info file or delay info file
*@param[in] - file_name: file path
*@param[out] - array: json object 
*@reval - error: -1 success: 0
*/
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


/**
*@brief: init a map with delay-info-list-file 
*@param[out] - delat_list: delay-info-list-file's path
*@param[in] - file_path: delay-info file path
*/
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
        std::string name_str = std::string(name);
        delay_list[name_str] = 0;
    }          
}

/**
*@brief: get delay_info from recieved message contains delays' name, 
         rate and etc.
*@param[out] - delay_info: structure with delay status information
*@param[in] - message: delay-info string recieved from delays
*/
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

/**
*@brief: compute weights of delay with delay's status information 
*@param[in] - delay status information
*@reval - delay's weight
*/
double compute_delay_weight(delay_info_t delay_info)
{
    double cpu_rate = delay_info.cpu_rate;
    double mem_total = delay_info.memory_total;
    double mem_used = delay_info.memory_total;
    double weight = (cpu_rate + (mem_used / mem_total)) / 2;
    return weight;
}


/**
*@brief: 
*@param[in] - delay_weight_list: list contains delays' weights 
*@param[out] - message: replay message to tor master
*/
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
	//printf("send: %s\n", msg);
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
    
    if (argc != 5) {
        fprintf(stderr, "Usage: %s /self/name/ /port/to/delay /port/to/tor_master /path/to/delay_list\n", argv[0]);
        return -1;
    }
    
    //--------------------------init the context------------------------------ 
    char *name = argv[1];
    char *port_to_delay = argv[2];
    char *port_to_dir = argv[3];
	char *path_to_delay_list = argv[4];
    
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
    //----------------------init the context done------------------------------ 
	

    //----------------load config file and init delay map---------------------- 
   
    //load json config file - delay info list stored nickname and weights
    std::map< std::string, double> delay_weight_list;
     
    init_delay_list_with_config_file(delay_weight_list, path_to_delay_list);
    //
	/*for(std::map<std::string, double>::iterator it = delay_weight_list.begin();
       it != delay_weight_list.end(); ++it) {
           std::cout << "node: " << it->first;
           std::cout << "-> weights: " << it->second;
           std::cout << std::endl;
    }*/
    int delay_nums;

    //collect delays' info when the server setup
    send_request_to_delays(delay_weight_list, 
						   router_to_delay, 
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
            send_request_to_delays(delay_weight_list, 
			                       router_to_delay, 
                                   "hehe", delay_nums);
            last_time = now_time;
        }
        
        //send weights info to all dirs need
        while(!dir_list.empty())
        {
            std::string id = dir_list.front();
            std::string message;
            get_delay_weight_string_from_list(delay_weight_list, 
											  message);
            send_to_one(router_to_dir, id.c_str(), 
			            message.c_str());
            dir_list.pop();
        }

        //get reply from delays
        zmq_poll (router_item, 2, 10);
        if (router_item[0].revents & ZMQ_POLLIN) 
        {
            std::string socket_id;
            std::string message;
            
            recieve_from_one(router_to_delay, socket_id, message);
            delay_info_t info;
            get_delay_info(info, message);
            dump_delay_info_t(info);
            
            double weight = compute_delay_weight(info);
            delay_weight_list[socket_id] = weight;
        }
        
        //recieve request from tor-master
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
