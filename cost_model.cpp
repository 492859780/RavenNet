#include"sync_plan.h"
#include"cost_model.h"
#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<cstring>
#include<queue>
#include<stack>
using namespace std;

//全局树
nf_node_t* root;
nf_node_t** nf_array;
memcpy_type_t READ_TYPE[GPU_IN] = {NEEDED,PACKET,POINTER};
memcpy_type_t WRITE_TYPE[CPU_IN] = {NEEDED,PACKET,NONE}; 

//用来记录遍历过程的栈和最小路径的栈
stack<nf_node_t*> global;
stack<nf_node_t*> target;
float global_cost = 1000000;
uint8_t counter = 0;
uint8_t cpu_dirty = 0;
uint8_t gpu_dirty = 0;

//这里的packet应该是不包含首部的
static float get_data_area_htod_cost(uint8_t plan)
{
    float cost = 0;
    uint32_t idx = 0;
    while(plan != 0)
    {
        idx++;
        if(plan & 1 == 1)
        {
            switch(idx)
            {
                case FLAG:
                    cost += FLAG_HTOD_COST;
                    break;
                case PAYLOAD:
                    cost += PAYLOAD_HTOD_COST;
                    break;
                case PROTOCAL:
                    cost += PROTOCAL_HTOD_COST;
                    break;
                case DSTPORT:
                    cost += PORT_HTOD_COST;
                    break;
                case SRCPORT:
                    cost += PORT_HTOD_COST;
                    break;
                case DSTIP:
                    cost += IP_HTOD_COST;
                    break;
                case SRCIP:
                    cost += IP_HTOD_COST;
                    break;
            }
        }
        plan >>= 1;
    }
    return cost;
}

static float get_data_area_dtoh_cost(uint8_t plan)
{
    float cost = 0;
    uint32_t idx = 0;
    while(plan != 0)
    {
        idx++;
        if(plan & 1 == 1)
        {
            switch(idx)
            {
                case FLAG:
                    cost += FLAG_DTOH_COST;
                    break;
                case PAYLOAD:
                    cost += PAYLOAD_DTOH_COST;
                    break;
                case PROTOCAL:
                    cost += PROTOCAL_DTOH_COST;
                    break;
                case DSTPORT:
                    cost += PORT_DTOH_COST;
                    break;
                case SRCPORT:
                    cost += PORT_DTOH_COST;
                    break;
                case DSTIP:
                    cost += IP_DTOH_COST;
                    break;
                case SRCIP:
                    cost += IP_DTOH_COST;
                    break;
            }
        }
        plan >>= 1;
    }
    return cost;
}

float get_cost(client_t* client)
{
    float cost = 0;
    uint32_t idx = 0;
    uint8_t read_hint = client->hint.read_hint >> (HINT_SIZE-DATA_AREAS);
    uint8_t write_hint = client->hint.write_hint >> (HINT_SIZE-DATA_AREAS);
    cost += get_data_area_htod_cost(read_hint);
    cost += get_data_area_dtoh_cost(write_hint);
    
    client->cost = cost;
    return cost;
}

//下述这种初始化方法还有一个问题：无法获取CUdeviceptr拷贝过程中，拷贝pkt的开销，这个开销需要在求值剪枝的过程中处理
static nf_node_t* init_node(nf_node_t* node,client_t client,int read_type,int write_type)
{
    node = new nf_node_t();
    node->gpu_buffer = WITHOUT_GPU_BUFFER;
    node->client.name = (char*)malloc(strlen(client.name));
    node->client.type = client.type;
    strcpy(node->client.name,client.name);
    if(client.type == GPU_NF)
    {
        node->read_type = (memcpy_type_s)read_type;
        node->write_type = (memcpy_type_s)write_type;
        node->client.hint.read_hint = client.hint.read_hint;
        node->client.hint.write_hint = client.hint.write_hint;
        float cost = 0;
        switch(read_type)
        {
            case NEEDED:
                cost += get_data_area_htod_cost(client.hint.read_hint >> (HINT_SIZE-DATA_AREAS));
                break;
            case PACKET: 
                cost += PKT_SIZE / HTOD_SPEED;
                break;
            case POINTER:
                cost += sizeof(char*)/HTOD_SPEED;
                break;
        }

        switch(write_type)
        {
            case NEEDED:
                cost += get_data_area_dtoh_cost(client.hint.write_hint >> (HINT_SIZE-DATA_AREAS));
                break;
            case PACKET: 
                cost += PKT_SIZE / DTOH_SPEED;
                break;
            case NONE:
                cost += 0;
                break;
        }

        node->client.hint.rw_hint = node->client.hint.read_hint << 8 | node->client.hint.write_hint;
        node->client.cost = cost;
    }
    else
    {
        //对于CPU来说，必定是不需要GPU_BUFFER的，其实其只存在needed这一种表示方式
        node->read_type = NEEDED;
        node->write_type = NEEDED;

        //并且其一定只访问我需要的内容，不存在拷贝进，拷贝出的问题，所有的同步操作，也都是在GPU_shared_buffer进行的
        node->client.hint.read_hint = client.hint.read_hint;
        node->client.hint.write_hint = client.hint.write_hint;
        node->client.hint.rw_hint = client.hint.read_hint << 8 | client.hint.write_hint;
        node->client.cost = 0; 
    }
         
    return node;
}
void show_tree(void)
{
    uint layers = -1;
    uint32_t len = 0;
    nf_node_t* node = NULL;
    queue<nf_node_t*> q;
    q.push(root);
    while(!q.empty())
    {
        len = q.size();
        printf("layer:%d\n",layers);
        for(uint32_t i = 0;i < len;i++)
        {
            node = q.front();
            q.pop();
            printf("cost:%f    ",node->client.cost);
            for(uint32_t j = 0;j < GPU_IN * CPU_IN ;j++)
            {
                if(node->next[j] != nullptr)
                    q.push(node->next[j]);
            }
        }
        printf("\n");
        layers++;
    }
}

//tree init process
//虽然CPU_NF其实所有的node都是双Needed，但是为了生成和遍历方便，我这里依旧生成了9个结点
nf_node_t* init_tree()
{
    //为了让layer能和client数组下标对应，因此初始化成-1
    //需要注意无符号和有符号不能比较大小
    int next_layer = -1;
    uint32_t len = 0;
    uint32_t nodes_num = 0;
    nf_node_t* node = NULL;
    root = new nf_node_t();

    queue<nf_node_t*> q;
    //边层次遍历边建树
    q.push(root);
    while(!q.empty() && next_layer < num_clients-1)
    {
        next_layer++;
        len = q.size();
        for(uint32_t k = 0;k < len;k++)
        {
            node = q.front();
            q.pop();
            for(uint32_t i = 0,counter = 0;i < GPU_IN;i++)
            {
                for(uint32_t j = 0;j < CPU_IN;j++,counter++)
                {    
                    node->next[counter] = init_node(node->next[counter],clients[next_layer],READ_TYPE[i],WRITE_TYPE[j]);
                    node->next[counter]->layers = next_layer;
                    q.push(node->next[counter]);
                    nodes_num += 1;     
                }
            }  
        }
        printf("layer:%d has %ld nodes\n",next_layer,q.size()); 
    }

    printf("total nodes in tree : %d\n",nodes_num);

    return nullptr;
}

static void Record_NF(float cost)
{
    int idx = num_clients - 1;
    stack<nf_node_t*> nodes = global;
    nf_array = new nf_node_t*[num_clients];
    nf_node_t* node;
    while(!nodes.empty() && idx >= 0)
    {
        node = nodes.top();
        printf("%s  NF_type:%s  buffer_type:%s  read_type:%s  writer_type:%s  hint:%d\n",node->client.name,GET_NF_TYPE(node->client.type),GET_BUFFER_TYPE(node->gpu_buffer),GET_MEMCPY_TYPE(node->read_type),GET_MEMCPY_TYPE(node->write_type),node->client.hint.rw_hint);
        nf_array[idx--] = node;
        nodes.pop();
    }
    create_sync_JSON(cost);
    delete [] nf_array;
}

static void Inorder_traverse(nf_node_t* node,nf_node_t* father,float* cost)
{   
    uint8_t tag = 0;
    uint8_t htod_flag = 0;
    uint8_t dtoh_flag = 0;
    uint8_t final_flag = 0;

    uint8_t f_cpu_dirty = cpu_dirty;
    uint8_t f_gpu_dirty = gpu_dirty;

    float htod_cost = 0;
    float dtoh_cost = 0;
    float final_cost = 0;

    counter ++;
    //对于跟结点来说，下面这些剪枝判断全都不是必须的，直接跳过
    if(node == father)
        goto traverse;

    //gpu_buffer handle
    if(node->read_type == PACKET && node->client.type == GPU_NF || father->gpu_buffer == WITH_GPU_BUFFER)
        node->gpu_buffer = WITH_GPU_BUFFER;
    else
        node->gpu_buffer = WITHOUT_GPU_BUFFER;

    if(node->gpu_buffer == WITHOUT_GPU_BUFFER && (node->read_type == POINTER || node->write_type == NONE) || node->write_type == NONE && node->client.hint.write_hint == FLAG_TAG)
        return ;

    //dtoh sync handle 所有sync handle全部都需要在gpu nf上进行
    //dtoh的同步机制修改的是gpu_dirty
    if(node->gpu_buffer == WITH_GPU_BUFFER && 
            (father->client.type == GPU_NF && (father->read_type != NEEDED || father->write_type != NEEDED)) && 
            (node->read_type == NEEDED && node->write_type == NEEDED))
    {
        tag = (uint8_t)(gpu_dirty & node->client.hint.read_hint);
        //printf("dtoh father gpu_dirty:%s current read_hint:%s\n",parse_binary_to_string(gpu_dirty,8).c_str(),parse_binary_to_string(node->client.hint.read_hint,8).c_str());
        if(tag != 0)
        {
            dtoh_flag = 1;
            father->client.plan.dtoh_plan = tag;
            dtoh_cost = get_data_area_dtoh_cost(tag >> (HINT_SIZE - DATA_AREAS));
            *cost += dtoh_cost;
            father->client.plan.sync_cost += dtoh_cost;
            gpu_dirty = gpu_dirty ^ tag;
        }
    }

    if(father->client.type == GPU_NF && father->write_type == PACKET)
        gpu_dirty = 0;

    //htod的同步处理
    //htod处理的是cpu_dirty
    if(node->gpu_buffer == WITH_GPU_BUFFER && (node->write_type != NEEDED || node->read_type != NEEDED) && 
            (father->read_type == NEEDED && father->write_type == NEEDED))
    {
        tag = (uint8_t)(cpu_dirty & node->client.hint.read_hint);
        //printf("htod father cpu_dirty:%s current read_hint:%s\n",parse_binary_to_string(cpu_dirty,8).c_str(),parse_binary_to_string(node->client.hint.read_hint,8).c_str());
        if(tag != 0)
        {
            htod_flag = 1;
            node->client.plan.htod_plan = tag;
            htod_cost = get_data_area_htod_cost(tag >> (HINT_SIZE - DATA_AREAS));
            *cost += htod_cost;
            node->client.plan.sync_cost += htod_cost;
            cpu_dirty = cpu_dirty ^ tag;
        }
    }
    
    if(node->client.type == GPU_NF && node->read_type == PACKET)
        cpu_dirty = 0;

    //这两种情况都是只修改了cpu_buffer的
    *cost += node->client.cost;
    if(node->client.type == CPU_NF || node->client.type == GPU_NF && node->read_type == NEEDED && node->write_type == NEEDED)
        cpu_dirty = (uint8_t)(cpu_dirty | (uint8_t)(node->client.hint.read_hint & node->client.hint.write_hint));
    else
        gpu_dirty = (uint8_t)(gpu_dirty | (uint8_t)(node->client.hint.read_hint & node->client.hint.write_hint));

    //printf("GPU dirty:%d read_type:%s write_type:%s\n",gpu_dirty,GET_MEMCPY_TYPE(node->read_type),GET_MEMCPY_TYPE(node->write_type));

traverse:
    global.push(node);
    if(node->next[0] == nullptr && node->next[1] == nullptr && node->next[2] == nullptr)
    {   
        //如果最后gpu_dirty中有脏位，则脏位需要记录给最后一个nf
        if(gpu_dirty != NONE_TAG)
        {
            final_flag = 1;
            final_cost = get_data_area_dtoh_cost(gpu_dirty >> (HINT_SIZE - DATA_AREAS));
            node->client.plan.dtoh_plan = gpu_dirty;
            node->client.plan.sync_cost += final_cost;
            *cost += final_cost;
        }

        if(*cost < global_cost)
        {    
            global_cost = *cost;
            target = global; 
        }
        Record_NF(*cost);
    }
    else{
        for(uint8_t i = 0;i < GPU_IN * CPU_IN;i++)
        {
            Inorder_traverse(node->next[i],node,cost);
        }         
    }

    //htod dtoh的开销没有重置
    global.pop();
    *cost -= node->client.cost;

    cpu_dirty = f_cpu_dirty;
    gpu_dirty = f_gpu_dirty;
    if(htod_flag)
        *cost -= htod_cost;
    if(dtoh_flag)
        *cost -= dtoh_cost;
    if(final_flag)
        *cost -= final_cost;
}

//首先先求出合适的最小cost路径，并与基本路径进行对比，选其中小的一个
//再据此，生成标准NFVs.json文件，include文件
void traverse_tree(void)
{
    int idx = num_clients-1;
    float cost = 0;
    Inorder_traverse(root,root,&cost);
    printf("%d nodes traverse successfully mini cost is %f\n",counter,global_cost);
}
