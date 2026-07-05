#ifndef CSI_SHARED_H
#define CSI_SHARED_H

#include <stdint.h>
#include "../include/dso_fifo.h"

typedef struct {
    int8_t   amp[64];
    uint8_t  valid;
} csi_sample_t;

extern dso_fifo _csi_fifo;

#endif
