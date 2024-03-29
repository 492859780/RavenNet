//这个文件专门用来写一些和gpu数据同步相关的数据结构和__device_函数
//主要用来面向nf使用
#ifndef _gpu_packet_sync_H_
#define _gpu_packet_sync_H_
#include<stdint.h>
#include<cuda.h>
#include<cuda_runtime.h>
#include"gpu_packet.h"

//Synchronization Global variable
#define SYNC_DATA_COUNT 5
#define SYNC_DATA_SIZE 16

//GPU同步数据类型
#define SYNC_SOURCE_IP   0
#define SYNC_DEST_IP     1
#define SYNC_SOURCE_PORT 2
#define SYNC_DEST_PORT   3
#define SYNC_PAYLOAD     4

//下面这两个结构是都需要传递给gpu的
//对于每个nf来说 全局当中需要使用到的同步数据结构
typedef struct packet_sync_global_s{
    uint8_t h2d_payload_flag;
    uint8_t d2h_payload_flag;

    //针对头文件的偏移
	CUdeviceptr h2d_offset_d;
	CUdeviceptr d2h_offset_d;
    //这里表示需要同步的数据大小
    uint16_t h2d_sync_size;
    uint16_t d2h_sync_size;
	uint16_t h2d_sync_num;
	uint16_t d2h_sync_num;

    //专门有一个payload_flag传递给gpu用来判断是否需要同步payload 
    int payload_size;
} __attribute__((aligned(16))) packet_sync_global_t;

//对于每个nf的batch来说 实际存储同步数据的结构
//sourceIP desIP sourcePort desPort Payload 一共13字节
typedef struct packet_sync_s{
    CUdeviceptr d_hdr_sync_h2d;
    CUdeviceptr d_hdr_sync_d2h;

    char* hdr_sync_h2d;
    char* hdr_sync_d2h;
}__attribute__((aligned(16))) packet_sync_t;

__inline__ __device__ 
void sync_h2d_header(uint16_t h2d_num,const uint16_t h2d_hdr_size,uint16_t* h2d_offset,
                    gpu_packet_t **input_buf,char* sync_in,
                    int id,int step,int job_num)
{
    uint16_t offset = 0;
    if(h2d_num != 0)
    {
        //gpkt可以直接通过偏移量来进行数据同步
        char data[SYNC_DATA_SIZE];
        for(int i = id ; i < job_num ; i += step){
            for(int j = 0;j < h2d_hdr_size; j++)
                data[j] = *(sync_in + i * h2d_hdr_size + j);
            
            for(int j = 0;j < h2d_num ;j++)
            {
                if(h2d_offset[j] <= 4)
                {
                    if(h2d_offset[j] == 0)
                        input_buf[i]->src_addr = *((uint32_t*)(data + offset));
                    else
                        input_buf[i]->dst_addr = *((uint32_t*)(data + offset));
                    offset += 4;
                }
                else if(h2d_offset[j] <= 10)
                {
                    if(h2d_offset[j]==10)
                        input_buf[i]->dst_port = *((uint16_t*)(data + offset));
                    else
                        input_buf[i]->src_port = *((uint16_t*)(data + offset));
                    offset += 2;
                }
            }
        }
    }
}

__inline__ __device__ 
void sync_d2h_header(uint16_t d2h_num,uint16_t d2h_hdr_size,uint16_t* d2h_offset,
                    gpu_packet_t **input_buf,char* sync_out,
                    int id,int step,int job_num)
{
    uint16_t offset = 0;
    if(d2h_num != 0)
    {
        char *data = NULL;
        for(int i = id ; i < job_num ; i += step){
            //得到起点拷贝数组
            data = sync_out + i * d2h_hdr_size;	
            for(int j = 0;j < d2h_num ;j++)
            {
                if(d2h_offset[j] <= 4)
                {    
                    *((uint32_t*)(data + offset)) = ((offset == 0) ? input_buf[i]->src_addr : input_buf[i]->dst_addr);
                    offset += 4;
                }
                else if(d2h_offset[j] <= 10)
                {    
                    *((uint16_t*)(data + offset)) = ((offset == 8) ? input_buf[i]->src_port : input_buf[i]->dst_port);
                    offset += 2;
                }
            }
        }
    }
}
#endif // !gpu_packet_sync_H_

