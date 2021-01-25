BUILD_PATH = ./build
cJSON_PATH ?= /usr/local/cJSON
APP = plan
CFLAGS += -O3 -g
CFLAGS += -I$(cJSON_PATH)
LD_FLAGS += -L$(cJSON_PATH)/build/lib -lcjson
OBJECTS = cost_model.o sync_plan.o main.o
CC = g++

build:$(APP) $(OBJECTS)
$(OBJECTS):%.o:%.cpp
	$(CC) -c $(CFLAGS) $< -o $@ 
$(APP):$(OBJECTS)
	$(CC)  $^ -o $@ $(LD_FLAGS)

.PHONY:clean
clean:
	#伪命令中的文件名是不需要""括起来的，可以直接使用通配符
	rm -rf  *.o
	rm -rf $(APP)
