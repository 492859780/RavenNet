#include"sync_plan.h"
#include"cost_model.h"
#include<stdio.h>
int main(void)
{
    load_nfv_json();
    printf("load json successfully\n");
    get_rw_hint();
    init_tree();

    //show_tree();

    traverse_tree();
    create_sync_JSON();
    return 0;
}