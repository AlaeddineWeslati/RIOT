#include <string.h>

#include "kernel.h"
#include "cpu-conf.h"
#include "cpu.h"

unsigned char *cpu_id(unsigned char *id, int *id_len)
{
#if defined(CPU_ID_LEN)

    if (*id_len > CPU_ID_LEN) {
        *id_len = CPU_ID_LEN;
    }

#if defined(CPU_ID_ADDR)
    memcpy(id, (void *)CPU_ID_ADDR, *id_len);

    return id;

#elif defined(CPU_ID)
    CPU_ID(id, *id_len);

    return id;

#else
    return NULL;
#endif
#else
    return NULL;
#endif
}
