#include <stdio.h>

#include "kernel.h"

int main(void)
{
    unsigned char id[12], *res;
    int id_len = 12;

    res = cpu_id(id, &id_len);

    if (res == &id[0]) {
        printf("CPU id is: ");

        for (int i = 0; i < id_len; i++) {
            printf("0x%02x ", id[i]);
        }

        printf("\n");
    }
    else if (res == NULL) {
        printf("No CPU id available.\n");
    }
    else {
        printf("Unexpected behavior: result of cpu_id() != &id[0] or NULL\n");
    }
}
