#ifndef _GET_SYSTEM_INFO_H_
#define _GET_SYSTEM_INFO_H_

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

typedef struct CPU_PACKED         //定义一个cpu occupy的结构体  
{  
    char name[20];             //定义一个char类型的数组名name有20个元素  
    unsigned int user;        //定义一个无符号的int类型的user  
    unsigned int nice;        //定义一个无符号的int类型的nice  
    unsigned int system;    //定义一个无符号的int类型的system  
    unsigned int idle;         //定义一个无符号的int类型的idle  
    unsigned int iowait;  
    unsigned int irq;  
    unsigned int softirq;  
}CPU_OCCUPY;  

typedef struct MEM_PACKED         //定义一个mem occupy的结构体  
{  
    char name[20];      //定义一个char类型的数组名name有20个元素  
    unsigned long total;  
    char name2[20];  
}MEM_OCCUPY;  

typedef struct MEM_PACK         //定义一个mem occupy的结构体  
{  
    double total,used_rate;  
}MEM_PACK; 


double get_cpu_rate();
MEM_PACK *get_memoccupy ();

#endif//_GET_SYSTEM_INFO_H_
