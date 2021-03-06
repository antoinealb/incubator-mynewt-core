/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <nrf52840.h>
#include "os/os_cputime.h"
#include "syscfg/syscfg.h"
#include "sysflash/sysflash.h"
#include "flash_map/flash_map.h"
#include "hal/hal_bsp.h"
#include "hal/hal_flash.h"
#include "hal/hal_spi.h"
#include "hal/hal_watchdog.h"
#include "hal/hal_i2c.h"
#include "mcu/nrf52_hal.h"
#include "uart/uart.h"
#include "uart_hal/uart_hal.h"
#include "os/os_dev.h"
#include "bsp.h"
#if MYNEWT_VAL(LSM303DLHC_PRESENT)
#include <lsm303dlhc/lsm303dlhc.h>
static struct lsm303dlhc lsm303dlhc;
#endif
#if MYNEWT_VAL(BNO055_PRESENT)
#include <bno055/bno055.h>
#endif
#if MYNEWT_VAL(TSL2561_PRESENT)
#include <tsl2561/tsl2561.h>
#endif
#if MYNEWT_VAL(TCS34725_PRESENT)
#include <tcs34725/tcs34725.h>
#endif

#if MYNEWT_VAL(LSM303DLHC_PRESENT)
static struct lsm303dlhc lsm303dlhc;
#endif

#if MYNEWT_VAL(BNO055_PRESENT)
static struct bno055 bno055;
#endif

#if MYNEWT_VAL(TSL2561_PRESENT)
static struct tsl2561 tsl2561;
#endif

#if MYNEWT_VAL(TCS34725_PRESENT)
static struct tcs34725 tcs34725;
#endif


#if MYNEWT_VAL(UART_0)
static struct uart_dev os_bsp_uart0;
static const struct nrf52_uart_cfg os_bsp_uart0_cfg = {
    .suc_pin_tx = MYNEWT_VAL(UART_0_PIN_TX),
    .suc_pin_rx = MYNEWT_VAL(UART_0_PIN_RX),
    .suc_pin_rts = MYNEWT_VAL(UART_0_PIN_RTS),
    .suc_pin_cts = MYNEWT_VAL(UART_0_PIN_CTS),
};
#endif

#if MYNEWT_VAL(UART_1)
static struct uart_dev os_bsp_bitbang_uart1;
static const struct uart_bitbang_conf os_bsp_uart1_cfg = {
    .ubc_txpin = MYNEWT_VAL(UART_1_PIN_TX),
    .ubc_rxpin = MYNEWT_VAL(UART_1_PIN_RX),
    .ubc_cputimer_freq = MYNEWT_VAL(OS_CPUTIME_FREQ),
};
#endif

#if MYNEWT_VAL(SPI_0_MASTER)
/*
 * NOTE: Our HAL expects that the SS pin, if used, is treated as a gpio line
 * and is handled outside the SPI routines.
 */
static const struct nrf52_hal_spi_cfg os_bsp_spi0m_cfg = {
    .sck_pin      = 45,
    .mosi_pin     = 46,
    .miso_pin     = 47,
};
#endif

#if MYNEWT_VAL(SPI_0_SLAVE)
static const struct nrf52_hal_spi_cfg os_bsp_spi0s_cfg = {
    .sck_pin      = 45,
    .mosi_pin     = 46,
    .miso_pin     = 47,
    .ss_pin       = 44,
};
#endif

#if MYNEWT_VAL(I2C_0)
static const struct nrf52_hal_i2c_cfg hal_i2c_cfg = {
    .scl_pin = 27,
    .sda_pin = 26,
    .i2c_frequency = 100    /* 100 kHz */
};
#endif

/*
 * What memory to include in coredump.
 */
static const struct hal_bsp_mem_dump dump_cfg[] = {
    [0] = {
        .hbmd_start = &_ram_start,
        .hbmd_size = RAM_SIZE
    }
};

const struct hal_flash *
hal_bsp_flash_dev(uint8_t id)
{
    /*
     * Internal flash mapped to id 0.
     */
    if (id != 0) {
        return NULL;
    }
    return &nrf52k_flash_dev;
}

const struct hal_bsp_mem_dump *
hal_bsp_core_dump(int *area_cnt)
{
    *area_cnt = sizeof(dump_cfg) / sizeof(dump_cfg[0]);
    return dump_cfg;
}

int
hal_bsp_power_state(int state)
{
    return (0);
}

/**
 * Returns the configured priority for the given interrupt. If no priority
 * configured, return the priority passed in
 *
 * @param irq_num
 * @param pri
 *
 * @return uint32_t
 */
uint32_t
hal_bsp_get_nvic_priority(int irq_num, uint32_t pri)
{
    uint32_t cfg_pri;

    switch (irq_num) {
    /* Radio gets highest priority */
    case RADIO_IRQn:
        cfg_pri = 0;
        break;
    default:
        cfg_pri = pri;
    }
    return cfg_pri;
}

#if MYNEWT_VAL(LSM303DLHC_PRESENT) || MYNEWT_VAL(BNO055_PRESENT)
static int
accel_init(struct os_dev *dev, void *arg)
{
   return (0);
}
#endif

#if MYNEWT_VAL(TSL2561_PRESENT)
static int
light_init(struct os_dev *dev, void *arg)
{
    return (0);
}
#endif

#if MYNEWT_VAL(TCS34725_PRESENT)
static int
color_init(struct os_dev *dev, void *arg)
{
    return (0);
}
#endif

static void
sensor_dev_create(void)
{
    int rc;

    (void)rc;
#if MYNEWT_VAL(LSM303DLHC_PRESENT)
    rc = os_dev_create((struct os_dev *) &lsm303dlhc, "accel0",
      OS_DEV_INIT_PRIMARY, 0, accel_init, NULL);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(BNO055_PRESENT)
    rc = os_dev_create((struct os_dev *) &bno055, "accel1",
      OS_DEV_INIT_PRIMARY, 0, accel_init, NULL);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(TSL2561_PRESENT)
    rc = os_dev_create((struct os_dev *) &tsl2561, "light0",
      OS_DEV_INIT_PRIMARY, 0, light_init, NULL);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(TCS34725_PRESENT)
    rc = os_dev_create((struct os_dev *) &tcs34725, "color0",
      OS_DEV_INIT_PRIMARY, 0, color_init, NULL);
    assert(rc == 0);
#endif
}

void
hal_bsp_init(void)
{
    int rc;

#if MYNEWT_VAL(TIMER_0)
    rc = hal_timer_init(0, NULL);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(TIMER_1)
    rc = hal_timer_init(1, NULL);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(TIMER_2)
    rc = hal_timer_init(2, NULL);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(TIMER_3)
    rc = hal_timer_init(3, NULL);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(TIMER_4)
    rc = hal_timer_init(4, NULL);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(TIMER_5)
    rc = hal_timer_init(5, NULL);
    assert(rc == 0);
#endif

#if (MYNEWT_VAL(OS_CPUTIME_TIMER_NUM) >= 0)
    rc = os_cputime_init(MYNEWT_VAL(OS_CPUTIME_FREQ));
    assert(rc == 0);
#endif

#if MYNEWT_VAL(I2C_0)
    rc = hal_i2c_init(0, (void *)&hal_i2c_cfg);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(SPI_0_MASTER)
    rc = hal_spi_init(0, (void *)&os_bsp_spi0m_cfg, HAL_SPI_TYPE_MASTER);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(SPI_0_SLAVE)
    rc = hal_spi_init(0, (void *)&os_bsp_spi0s_cfg, HAL_SPI_TYPE_SLAVE);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(UART_0)
    rc = os_dev_create((struct os_dev *) &os_bsp_uart0, "uart0",
      OS_DEV_INIT_PRIMARY, 0, uart_hal_init, (void *)&os_bsp_uart0_cfg);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(UART_1)
    rc = os_dev_create((struct os_dev *) &os_bsp_bitbang_uart1, "uart1",
      OS_DEV_INIT_PRIMARY, 0, uart_bitbang_init, (void *)&os_bsp_uart1_cfg);
    assert(rc == 0);
#endif

    sensor_dev_create();
}
