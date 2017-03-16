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
#include <assert.h>
#include "syscfg/syscfg.h"
#include "os/os_cputime.h"
#include "controller/ble_phy.h"
#include "controller/ble_ll.h"
#include "controller/ble_ll_xcvr.h"

#if 0
#include <string.h>
#include "sysinit/sysinit.h"
#include "os/os.h"
#include "stats/stats.h"
#include "bsp/bsp.h"
#include "nimble/ble.h"
#include "nimble/nimble_opt.h"
#include "nimble/hci_common.h"
#include "nimble/ble_hci_trans.h"
#include "controller/ble_hw.h"
#include "controller/ble_ll_adv.h"
#include "controller/ble_ll_sched.h"
#include "controller/ble_ll_scan.h"
#include "controller/ble_ll_hci.h"
#include "controller/ble_ll_whitelist.h"
#include "controller/ble_ll_resolv.h"
#include "ble_ll_conn_priv.h"
#endif

#ifdef BLE_XCVR_RFCLK
int
ble_ll_xcvr_rfclk_state(void)
{
    uint32_t expiry;

    if (g_ble_ll_data.ll_rfclk_state == BLE_RFCLK_STATE_ON) {
        expiry = g_ble_ll_data.ll_rfclk_start_time;
        if ((int32_t)(os_cputime_get32() - expiry) >
                g_ble_ll_data.ll_xtal_ticks) {
            g_ble_ll_data.ll_rfclk_state = BLE_RFCLK_STATE_SETTLED;
        }
    }
    return g_ble_ll_data.ll_rfclk_state;
}

/**
 * Start the rf clock timer running at the specified cputime. The cputime
 * is when the clock should start; it will be settled after xtal ticks have
 * expired.
 *
 * If the clock is ON or SETTLED there is no need to start the timer. If the
 * clock is OFF the timer might be set for some time in the future. If it is,
 * we will reset the time if 'cputime' is earlier than the expiry time.
 *
 * @param cputime
 */
void
ble_ll_xcvr_rfclk_start(uint32_t cputime)
{
    if (g_ble_ll_data.ll_rfclk_state == BLE_RFCLK_STATE_OFF) {

        /*
         * If the timer is on the list, we need to see if its expiry is before
         * 'cputime'. If the expiry is before, no need to do anything. If it
         * is after, we need to stop the timer and start at new time.
         */
        if (g_ble_ll_data.ll_rfclk_timer.link.tqe_prev != NULL) {
            if ((int32_t)(cputime - g_ble_ll_data.ll_rfclk_timer.expiry) >= 0) {
                return;
            }
            os_cputime_timer_stop(&g_ble_ll_data.ll_rfclk_timer);
        }
        os_cputime_timer_start(&g_ble_ll_data.ll_rfclk_timer, cputime);
        ble_ll_log(BLE_LL_LOG_ID_RFCLK_START, g_ble_ll_data.ll_rfclk_state, 0,
                   g_ble_ll_data.ll_rfclk_timer.expiry);
    }
}

void
ble_ll_xcvr_rfclk_enable(void)
{
    g_ble_ll_data.ll_rfclk_state = BLE_RFCLK_STATE_ON;
    ble_phy_rfclk_enable();
}

void
ble_ll_xcvr_rfclk_disable(void)
{
    ble_phy_rfclk_disable();
    g_ble_ll_data.ll_rfclk_state = BLE_RFCLK_STATE_OFF;
}

void
ble_ll_xcvr_rfclk_stop(void)
{
    ble_ll_log(BLE_LL_LOG_ID_RFCLK_STOP, g_ble_ll_data.ll_rfclk_state, 0,0);
    os_cputime_timer_stop(&g_ble_ll_data.ll_rfclk_timer);
    ble_ll_xcvr_rfclk_disable();
}

uint32_t
ble_ll_xcvr_rfclk_time_till_settled(void)
{
    int32_t dt;
    uint32_t rc;

    rc = 0;
    if (g_ble_ll_data.ll_rfclk_state == BLE_RFCLK_STATE_ON) {
        dt = (int32_t)(os_cputime_get32() - g_ble_ll_data.ll_rfclk_start_time);
        assert(dt >= 0);
        if (dt < g_ble_ll_data.ll_xtal_ticks) {
            rc = g_ble_ll_data.ll_xtal_ticks - (uint32_t)dt;
        }
    }

    return rc;
}

/**
 * Called when the timer to turn on the RF CLOCK expires
 *
 * Context: Interrupt
 *
 * @param arg
 */
void
ble_ll_xcvr_rfclk_timer_exp(void *arg)
{
    if (g_ble_ll_data.ll_rfclk_state == BLE_RFCLK_STATE_OFF) {
        ble_ll_xcvr_rfclk_enable();
        g_ble_ll_data.ll_rfclk_start_time = g_ble_ll_data.ll_rfclk_timer.expiry;
        ble_ll_log(BLE_LL_LOG_ID_RFCLK_ENABLE, g_ble_ll_data.ll_rfclk_state, 0,
                   g_ble_ll_data.ll_rfclk_start_time);
    }
}

/**
 * This API is used to turn on the rfclock without setting the cputimer timer
 * to start the clock at some later point.
 *
 * @param now
 */
void
ble_ll_xcvr_rfclk_start_now(uint32_t now)
{
    ble_ll_xcvr_rfclk_enable();
    g_ble_ll_data.ll_rfclk_start_time = now;
    ble_ll_log(BLE_LL_LOG_ID_RFCLK_ENABLE, g_ble_ll_data.ll_rfclk_state, 0,
               g_ble_ll_data.ll_rfclk_start_time);
}

/* WWW: This is really confusing. This is called when we add something
 * to schedule at the start. We want to stop the current cputime timer
 * for the clock and restart it at the new time.
 *
 */
void
ble_ll_rfclk_start(uint32_t cputime)
{
    /* If we are currently doing something, no need to start the clock */
    if (g_ble_ll_data.ll_state == BLE_LL_STATE_STANDBY) {
        ble_ll_xcvr_rfclk_start(cputime - g_ble_ll_data.ll_xtal_ticks);
    }
}
#endif
