# G-NET sync plan辅助程序
该程序主要完成如下两个工作
1. 根据给出的基本sync_plan.json，求出不同server_chain最小cost的标准sync_plan
2. 根据标准sync_plan去求出每个NF的头文件onvm_tags.h，包括了如下8个域
    a. gpu_buffer_tag
    b. read_type
    c. write_type
    d. 各area的htod_plan
    e. 各area的dtoh_plan
    f. 各area的read_plan
    j. 各area的write_plan
    h. nfv_type