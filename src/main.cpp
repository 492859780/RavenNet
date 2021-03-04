#include"sync_plan.h"
#include"cost_model.h"
#include<stdio.h>
int main(void)
{
    load_nfv_json();
    printf("load json successfully\n");
    get_rw_hint();
    init_tree();
    
    double cost = traverse_tree();

#ifndef RECORD_ALL_RESULTS
    Record_NF(cost);
#endif

    return 0;
}