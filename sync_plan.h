#ifndef _sync_plan_H_
#define _sync_plan_H_
#include<stdint.h>
#include<string>
#define DATA_AREAS 7
#define HINT_SIZE 8
#define MAX_CLIENTS 16

//读写hint
typedef struct rw_hint_s{
    uint8_t read_hint;
    uint8_t write_hint;
    uint16_t rw_hint; 

    //constructor
    rw_hint_s():read_hint(0),write_hint(0),rw_hint(0){} 
} rw_hint_t;

//同步计划方案
typedef struct sync_plan_s{
    uint8_t htod_plan;
    uint8_t dtoh_plan;
    uint16_t sync_plan;
    
    //constructor
    sync_plan_s():htod_plan(0),dtoh_plan(0),sync_plan(0){}
}sync_plan_t;

typedef enum NF_type_s{
    CPU_NF,
    GPU_NF
}NF_type_t;
#define GET_NF_TYPE(type) type == 0 ? "CPU_NF" : "GPU_NF"
//每个NF的信息
typedef struct client_s{
    rw_hint_t hint;
    sync_plan_t plan;

    float cost;
    char* name;
    //if type == 1 then GPU nf else CPU nf
    uint8_t type;

    //constructor
    client_s(){
        name = nullptr;
        cost = 0;
        type = GPU_NF;
    }
}client_t;

//8字节其中使用6个字节 分别表示srcIP dstIP srcPort dstPort Protocol Payload
extern int num_clients;
extern uint8_t CR_hints[MAX_CLIENTS];
extern uint8_t CW_hints[MAX_CLIENTS];
extern uint8_t GR_hints[MAX_CLIENTS];
extern uint8_t GW_hints[MAX_CLIENTS];
extern uint16_t hint[MAX_CLIENTS];
extern client_t clients[MAX_CLIENTS];

std::string parse_binary_to_string(uint16_t hint,const int len);
uint8_t parse_str_to_binary(char* hint);

int load_nfv_json(void);
void get_rw_hint(void);
/*
使用下面一种算法得出的sync/_plan假设了同步机制，就意味着有一个公共的shared_buffer
然而实际上我不希望有这么一个shared_buffer，因此需要一个新的函数needed_plan
得到G-NET中的同步数据域
*/
void get_sync_plan(void);

//创建json和编译文件
void create_sync_JSON(void);
void create_compile_file(void);
#endif  