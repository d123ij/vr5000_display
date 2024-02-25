#ifndef VR5000_DISP_H
#define VR5000_DISP_H
#pragma once

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "hardware/dma.h"
#include "hardware/timer.h"
#include "hardware/spi.h"
#include "disp_func.h"

#include "vr5000_disp.pio.h"

#define cDMA_BUF_SZ           32 //capture buffer size, 128 bytes
#define CAPTURE_RING_BITS     7  //7 bits are needed for 32*4 = 128 bytes
#define cDMA_QUEUE_SZ         8192

#endif 