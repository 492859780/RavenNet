#ifndef _cost_model_H_
#define _cost_model_H_

#include"sync_plan.h"

//这里速度的单位是GB/s 相当于 B/ns 最终得到的时间大小是以纳秒为单位的
#define HTOD_SPEED 12.4
#define DTOH_SPEED 13.1
#define PKT_SIZE 1518 //64-1518表示以太网数据帧(包括首部和尾部)的大小范围
#define IP_SIZE 4
#define IP_HTOD_COST (IP_SIZE/HTOD_SPEED)
#define IP_DTOH_COST (IP_SIZE/DTOH_SPEED)
#define PORT_SIZE 2
#define PORT_HTOD_COST (PORT_SIZE/HTOD_SPEED)
#define PORT_DTOH_COST (PORT_SIZE/DTOH_SPEED)
#define PROTOCAL_SIZE 1
#define PROTOCAL_HTOD_COST (PROTOCAL_SIZE/HTOD_SPEED)
#define PROTOCAL_DTOH_COST (PROTOCAL_SIZE/DTOH_SPEED)
#define FLAG_SIZE 1
#define FLAG_HTOD_COST (FLAG_SIZE/HTOD_SPEED)
#define FLAG_DTOH_COST (FLAG_SIZE/DTOH_SPEED)
#define PAYLOAD_HTOD_COST ((PKT_SIZE - 20 - 18 - 8) / HTOD_SPEED)
#define PAYLOAD_DTOH_COST ((PKT_SIZE - 20 - 18 - 8) / DTOH_SPEED)

#define FLAG_TAG 0b00000010
#define NONE_TAG 0b00000000
#define PKT_TAG  0b11111100

#define GPU_IN 3
#define CPU_IN 3

typedef enum data_area_s{
    FLAG = 1,
    PAYLOAD,
    PROTOCAL,
    DSTPORT,
    SRCPORT,
    DSTIP,
    SRCIP
}data_area_t;

typedef enum memcpy_type_s{
    NEEDED,
    PACKET,
    POINTER,
    NONE
}memcpy_type_t;
#define GET_MEMCPY_TYPE(type) type == 0 ? "NEEDED" : (type == 1 ? "PACKET" : (type == 2 ? "POINTER" : "NONE"))

typedef enum gpu_buffer_s{
    WITH_GPU_BUFFER,
    WITHOUT_GPU_BUFFER
}gpu_buffer_t;
#define GET_BUFFER_TYPE(type) type == 0 ? "WITH_GPU_BUFFER" : "WITHOUT_GPU_BUFFER"

typedef struct nf_node_s{
    client_t client;
    int layers;
    uint8_t read_type;
    uint8_t write_type;
    //gpu_buffer表示当前NF是不是处于拥有GPU_buffer的情况，这个变量应该在遍历树的时候，根据是否进行pkt cpy来生成
    uint8_t gpu_buffer;
    struct nf_node_s* next[GPU_IN * CPU_IN];

    //constructor
    nf_node_s(){
        layers = -1;
        read_type = 0;
        write_type = 0;
        //初始化的时候默认是都没有GPU_BUFFER的
        gpu_buffer = WITHOUT_GPU_BUFFER;
        for(int i = 0;i < GPU_IN;i++)
            next[i] = nullptr;
    }
}nf_node_t;

float get_cost(client_t *client);

//树相关操作
nf_node_t* init_tree(void);
void show_tree(void);
float traverse_tree(void);
#endif