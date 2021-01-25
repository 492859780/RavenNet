#include"sync_plan.h"
#include"cost_model.h"
#include<cjson/cJSON.h>
#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<cstring>
#include<string>
#include<iostream>
#include<sstream>
using namespace std;

int num_clients = 0;
uint8_t CR_hints[MAX_CLIENTS];
uint8_t CW_hints[MAX_CLIENTS];
uint8_t GR_hints[MAX_CLIENTS];
uint8_t GW_hints[MAX_CLIENTS];
uint16_t hint[MAX_CLIENTS];
client_t clients[MAX_CLIENTS];

extern nf_node_t** nf_array;
extern float global_cost;

uint8_t parse_str_to_binary(char* hint)
{
	uint8_t ht = 0;
	for (size_t i = 0; i < HINT_SIZE && atoi(hint) != 0; i++)
	{
		ht |= ((hint[i] - '0')<<(HINT_SIZE - i - 1));
	}
	return ht;
}

//load json文件，填充4个hints数组，并且将nf名称信息和type信息写入clients数组
int load_nfv_json(void)
{
	FILE* fp = fopen("./NFVs.json","r+");
	if(fp == NULL)
	{
		printf("No json file\n");
		return -1;
	}

	printf("Open Json successfully\n");

	char buffer[64];
	char* JSON_str = NULL;

	size_t size = 1;
	while( fgets(buffer,64,fp) != NULL )
	{
		size = sizeof(buffer);
		if(JSON_str == NULL)
			JSON_str = (char*)malloc(size);
		else
		{
			JSON_str = (char*)realloc(JSON_str,strlen(JSON_str)+size);
			//这一步取大小不能用sizeof，因为取出来的会是char*的大小为8字节
		}
		strcat(JSON_str,buffer);
	}
	
	//printf("\n%s\n",JSON_str);
    cJSON* json = cJSON_Parse(JSON_str);
    if(json == NULL)
    {
		printf("Can't parse json\n");
        return -1;
    }

    const cJSON* NF_number = NULL;
    const cJSON* NFVs = NULL;
    const cJSON* NF = NULL;

	NF_number = cJSON_GetObjectItemCaseSensitive(json,"NFV_Number");
	if(NF_number == NULL)
	{	
		printf("Cant get NFV number,probably not setting the value in json\n");
		return -1;
	}

	num_clients = atoi(NF_number->valuestring);
	printf("Number of nfvs is %d\n",num_clients);

	NFVs= cJSON_GetObjectItemCaseSensitive(json,"NFVs");
	if(NFVs == NULL)
	{	
		printf("Cant get NFV Array,probably not setting the value in json\n");
		return -1;
	}
	printf("get NFs successfully\n");

	int idx = 0;
	cJSON_ArrayForEach(NF,NFVs)
	{
		//需要注意，C/C++多个指针是如何同时声明的
		const cJSON* name = NULL;
		const cJSON* CR = NULL , *CW = NULL;
		const cJSON* GR = NULL , *GW = NULL;
		const cJSON* type = NULL;

		name = cJSON_GetObjectItemCaseSensitive(NF,"Name");
		CR = cJSON_GetObjectItemCaseSensitive(NF,"CR_Hint");    
		GR = cJSON_GetObjectItemCaseSensitive(NF,"GR_Hint");    
		CW = cJSON_GetObjectItemCaseSensitive(NF,"CW_Hint");    
		GW = cJSON_GetObjectItemCaseSensitive(NF,"GW_Hint");    
		type = cJSON_GetObjectItemCaseSensitive(NF,"Type");

		if(clients[idx].name == NULL)
			clients[idx].name = (char*)malloc(sizeof(name->valuestring));
		
		strcpy(clients[idx].name,name->valuestring);
		CR_hints[idx] = parse_str_to_binary(CR->valuestring);
		GR_hints[idx] = parse_str_to_binary(GR->valuestring);
		CW_hints[idx] = parse_str_to_binary(CW->valuestring);
		GW_hints[idx] = parse_str_to_binary(GW->valuestring);
		clients[idx].type = ((strcmp(type->valuestring,"GPU") == 0) ? GPU_NF : CPU_NF);
		idx++;
	}

	printf("NF service chain\n");
	for(size_t i = 0 ; i < num_clients ; i++)
	{
		printf("NF %ld is %s\n",i,clients[i].name);
	}

	cJSON_Delete(json);
	return 1;
}

void get_rw_hint(void)
{
	uint16_t RV[MAX_CLIENTS];//GR|CR
    uint16_t WV[MAX_CLIENTS];//CW|GW

	for (int i = 0; i < num_clients; i++) {	
		//h2d|d2h
		if(clients[i].type == GPU_NF)
		{
			RV[i]=GR_hints[i]<<8|CR_hints[i];
			WV[i]=CW_hints[i]<<8|GW_hints[i];
		}
		else
		{
			RV[i]=CR_hints[i]<<8|GR_hints[i];
			WV[i]=GW_hints[i]<<8|CW_hints[i];
		}
		hint[i]=RV[i]|WV[i];
	}

	for(int i = 0;i < num_clients;i++)
	{	
		clients[i].hint.rw_hint = hint[i];
		clients[i].hint.read_hint = hint[i] >> 8;
		clients[i].hint.write_hint = hint[i] & 0xFF;
		if(clients->type == GPU_NF)
			clients[i].cost = get_cost(&clients[i]);
		else
			clients[i].cost = 0;
		printf("nf %d    hint:%d    read_hint:%d    write_hint:%d    cost:%f\n",i,hint[i],clients[i].hint.read_hint,clients[i].hint.write_hint,clients[i].cost);
	}
}

void get_sync_plan(void)
{
    uint8_t LMASK = 0xFF;
    uint16_t HMASK = 0xFFFF;
    uint16_t dirty = 0;
    uint16_t RV[MAX_CLIENTS];//GR|CR
    uint16_t WV[MAX_CLIENTS];//CW|GW
	uint16_t curPlan = 0;//这个current plan是用来表示最后加入nf服务链的client对应的plan

	int instance0 = 0;
	hint[instance0] = 0;

	for (int i = 0; i < num_clients; i++) {	
		//h2d|d2h
		RV[i]=GR_hints[i]<<8|CR_hints[i];
		WV[i]=CW_hints[i]<<8|GW_hints[i];

		hint[instance0]=hint[instance0]|(RV[i]&HMASK)|(WV[i]<<8);
	}

	curPlan = hint[instance0];
	dirty = WV[instance0];

	//接下来，这里要从第2个NF开始
	for(int i = 1;i < num_clients; i++)
	{
		hint[i]=dirty & RV[i];
		dirty=(dirty ^ hint[i])|WV[i];
	}

	hint[num_clients - 1] = dirty & LMASK;

	//在这个过程中填充clients数组，并且计算标准sync_plan的开销
	for(int i = 0;i < num_clients;i++)
	{	
		printf("nf %d  plan %d\n",i,hint[i]);
	}
}

//下面这几个函数负责把Hint相关信息再写入json文件
/*
需要写入的内容有
NF_NAME:
TYPE:
BUFFER_TYPE:
HTOD_TYPE:
DTOH-TYPE:
SYNC_PLAN:
*/

string parse_binary_to_string(uint16_t hint,const int len)
{
	char str[len+1];
	uint8_t tp = 0;
	string out = "";
	for(uint16_t idx = 0;idx < len;idx++)
	{ 
		tp = (int)((uint8_t)hint & 0b1) + '0';
		str[len - 1 - idx] = (char)tp;
		hint >>= 1;		
	}
	out = str;
	//如果不给出\0可能会出问题
	out[len] = '\0';
	//cout<<out<<endl;
	return out;
}

static inline void add_item_to_json(string* str,string key,int value)
{
	char out[1024];
	string fmt = "\t\"%s\" : \"%s\",\n";
	snprintf(out,sizeof(out),fmt.c_str(),key.c_str(),to_string(value).c_str());
	string tp = out;
	*str += tp;
}

static inline void add_item_to_json(string* str,string key,double value)
{
	char out[1024];
	string fmt = "\t\"%s\" : \"%s\",\n";
	snprintf(out,sizeof(out),fmt.c_str(),key.c_str(),to_string(value).c_str());
	string tp = out;
	*str += tp;
}

static inline void add_nf_to_json(string* str,string key,nf_node_t** arrays,int len)
{
	char out[2048];
	string fmt_array = "\t\"%s\" : [%s\n\t\t]\n";
	string fmt = "\t\t\t\t\"%s\":\"%s\"";
	string value_str = "";
	for(int i = 0;i < len;i++)
	{
		char str[1024];
		string tp = "\n\t\t\t{\n";
		snprintf(str,sizeof(str),fmt.c_str(),"Name",arrays[i]->client.name);
		tp += str;
		tp += ",\n";
		snprintf(str,sizeof(str),fmt.c_str(),"Type",GET_NF_TYPE(arrays[i]->client.type));
		tp += str;
		tp += ",\n";
		snprintf(str,sizeof(str),fmt.c_str(),"Cost(ns)",to_string(arrays[i]->client.cost).c_str());
		tp += str;
		tp += ",\n";
		snprintf(str,sizeof(str),fmt.c_str(),"BufferType",GET_BUFFER_TYPE(arrays[i]->gpu_buffer));
		tp += str;
		tp += ",\n";
		snprintf(str,sizeof(str),fmt.c_str(),"ReadType",GET_MEMCPY_TYPE(arrays[i]->read_type));
		tp += str;
		tp += ",\n";
		snprintf(str,sizeof(str),fmt.c_str(),"WriteType",GET_MEMCPY_TYPE(arrays[i]->write_type));
		tp += str;
		tp += ",\n";
		snprintf(str,sizeof(str),fmt.c_str(),"RW_Hint",parse_binary_to_string(arrays[i]->client.hint.rw_hint,16).c_str());
		tp += str;
		tp += "\n";
		/*
		snprintf(str,sizeof(str),fmt.c_str(),"Htod_SyncPlan",parse_binary_to_string(arrays[i]->client.plan.htod_plan,8).c_str());
		tp += str;
		tp += ",\n";
		snprintf(str,sizeof(str),fmt.c_str(),"DtoH_SyncPlan",parse_binary_to_string(arrays[i]->client.plan.dtoh_plan,8).c_str());
		tp += str;
		tp += "\n";
		*/
		tp += "\t\t\t}";
		if(i != len -1)
			tp += ",";
		value_str += tp;
	}
	snprintf(out,sizeof(out),fmt_array.c_str(),key.c_str(),value_str.c_str());
	string tp = out;
	*str += tp;
}

void create_sync_JSON(void)
{
	string str = (char*)"{\n";
	add_item_to_json(&str,"NFV_Number",num_clients);
	add_item_to_json(&str,"SC_Cost(ns)",(double)global_cost);
	add_nf_to_json(&str,"NFVs",nf_array,num_clients);
	str += "\n}\n";
	printf("%s",str.c_str());

	FILE* fp = fopen("./standard_nfvs.json","w+");
	fputs(str.c_str(),fp);
	fclose(fp);
}