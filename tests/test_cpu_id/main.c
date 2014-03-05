#include <stdio.h>

#include "cpu-conf.h"
#include "kernel.h"

int main(void)
{
    cpu_id_t id;

    GET_CPU_ID(id);

    printf("CPU id is: ");

    for (unsigned int i = 0; i < CPU_ID_LEN; i++) {
        printf("0x%02x ", id.id[i]);
    }

    printf("\n");
}
