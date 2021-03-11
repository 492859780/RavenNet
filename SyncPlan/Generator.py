import os
import json
SIZE = 128
PATH = "./standard_nfvs.json"
AREA = ["SRC_IP","DST_IP","SRC_PORT","DST_PORT","PROTOCOL","PAYLOAD","FLAG"]

def get_sync_plan(plan,out_area):
    for i in range(len(plan) - 1):
        if plan[i] != '0':
            out_area.append(AREA[i])
    return out_area

def get_rdwr_area(area,read_area,write_area):
    area_len = len(area)
    for i in range(int(area_len/2)):
        if area[i] != '0':
            read_area.append(AREA[i])

    for i in range(int(area_len/2),area_len):
        if area[i] != '0':
            write_area.append(AREA[i - int(area_len/2)])

    return read_area,write_area


if __name__ == "__main__":
    output_txt = "#ifndef _ONVM_TAGS_H_\n#define _ONVM_TAGS_H_\n"
    if not os.path.exists(PATH):
        exit(1)
    
    tp = ""
    data = ""

    with open(PATH,"r+",encoding="utf-8") as fd:
        tp = fd.read(SIZE)
        if len(tp) == 0:
            print("File no data\n")
            exit(1)

        while len(tp) > 0:

            data += tp
            tp = fd.read(SIZE)
    
    data = json.loads(data)
    nfv_num = data["NFV_Number"]
    nfvs = data["NFVs"]

    for nfv in nfvs:
        name = nfv["Name"][3:].upper()
        nfv_type = nfv["Type"]
        buffer_type = nfv["BufferType"]
        read_type = nfv["ReadType"]
        write_type = nfv["WriteType"]
        rw_hint = nfv["RW_Hint"]
        htod_plan = nfv["Htod_SyncPlan"]    
        dtoh_plan = nfv["Dtoh_SyncPlan"]    

        output_txt += "\n#define " + name + "_" + nfv_type[0:3]
        output_txt += "\n#define " + name + "_" + buffer_type 
        output_txt += "\n#define " + name + "_RD_" + read_type
        output_txt += "\n#define " + name + "_WR_" + read_type

        out_area = []
        out_area = get_sync_plan(htod_plan,out_area)
        if len(out_area) != 0:
            for _ in out_area:
                output_txt += "\n#define " + name + "_HtoD_" + _ 

        out_area.clear()
        out_area = get_sync_plan(dtoh_plan,out_area)
        if len(out_area) != 0:
            for _ in out_area:
                output_txt += "\n#define " + name + "_DtoH_" + _ 

        read_area = []
        write_area = []
        read_area,write_area = get_rdwr_area(rw_hint,read_area,write_area)
        if len(read_area) != 0:
            for _ in read_area:
                output_txt += "\n#define " + name + "_RD_" + _ 

        if len(write_area) != 0:
            for _ in write_area:
                output_txt += "\n#define " + name + "_WR_" + _      

        output_txt += "\n"

    output_txt += "\n#endif"

    with open("./onvm_tags.h","w+") as fd:
        fd.write(output_txt)

