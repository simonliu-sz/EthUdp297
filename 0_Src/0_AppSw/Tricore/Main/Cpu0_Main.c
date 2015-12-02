/**
 * \file Cpu0_Main.c
 * \brief System initialisation and main program implementation.
 *
 * \version iLLD_Demos_1_0_0_0_0
 * \copyright Copyright (c) 2014 Infineon Technologies AG. All rights reserved.
 *
 *
 *                                 IMPORTANT NOTICE
 *
 *
 * Infineon Technologies AG (Infineon) is supplying this file for use
 * exclusively with Infineon's microcontroller products. This file can be freely
 * distributed within development tools that are supporting such microcontroller
 * products.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS".  NO WARRANTIES, WHETHER EXPRESS, IMPLIED
 * OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE.
 * INFINEON SHALL NOT, IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL,
 * OR CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 */

/******************************************************************************/
/*----------------------------------Includes----------------------------------*/
/******************************************************************************/

#include "Cpu0_Main.h"
#include "SysSe/Bsp/Bsp.h"
#include "EthDemo.h"
#include "Ifx_Lwip.h"
#include "IfxPort_Io.h"
#include "IfxPort_cfg.h"

extern IfxEth	Ifx_g_Eth;

#define TRANSPORT_HEADERS_SIZE (PBUF_LINK_HLEN + PBUF_IP_HLEN + PBUF_TRANSPORT_HLEN)

const IfxPort_Io_ConfigPin	 configPin[] = {
		{&IfxPort_P33_6,  IfxPort_Mode_outputPushPullGeneral, IfxPort_PadDriver_cmosAutomotiveSpeed1},              // P00.0
		{&IfxPort_P33_7,  IfxPort_Mode_outputPushPullGeneral, IfxPort_PadDriver_cmosAutomotiveSpeed1},              // P00.0
};

const IfxPort_Io_Config conf = {
		sizeof(configPin)/sizeof(IfxPort_Io_ConfigPin),
		(IfxPort_Io_ConfigPin *)configPin
};

/******************************************************************************/
/*------------------------Inline Function Prototypes--------------------------*/
/******************************************************************************/

/******************************************************************************/
/*-----------------------------------Macros-----------------------------------*/
/******************************************************************************/

/******************************************************************************/
/*------------------------Private Variables/Constants-------------------------*/
/******************************************************************************/

/******************************************************************************/
/*------------------------------Global variables------------------------------*/
/******************************************************************************/
App_Cpu0 g_AppCpu0; /**< \brief CPU 0 global data */

/******************************************************************************/
/*-------------------------Function Implementations---------------------------*/
/******************************************************************************/

/** \brief Main entry point after CPU boot-up.
 *
 *  It initialise the system and enter the endless loop that handles the demo
 */
int core0_main(void)
{
    udp_pcb_t * udp;
    ip_addr_t addr;
    pbuf_t *p = (void*)0;//(pbuf_t *)pbuf_alloc_special(MEMP_PBUF);
    void *ethRam;
    uint16 idx;
    uint16 total;

    /*
     * !!WATCHDOG0 AND SAFETY WATCHDOG ARE DISABLED HERE!!
     * Enable the watchdog in the demo if it is required and also service the watchdog periodically
     * */
    IfxScuWdt_disableCpuWatchdog(IfxScuWdt_getCpuWatchdogPassword());
    IfxScuWdt_disableSafetyWatchdog(IfxScuWdt_getSafetyWatchdogPassword());

    /* Initialise the application state */
    g_AppCpu0.info.pllFreq = IfxScuCcu_getPllFrequency();
    g_AppCpu0.info.cpuFreq = IfxScuCcu_getCpuFrequency(IfxCpu_getCoreId());
    g_AppCpu0.info.sysFreq = IfxScuCcu_getSpbFrequency();
    g_AppCpu0.info.stmFreq = IfxStm_getFrequency(&MODULE_STM0);


    IfxPort_Io_initModule(&conf);
    for (idx = 0; idx < conf.size; idx++)
    {
    	IfxPort_Io_ConfigPin *tbl = &conf.pinTable[idx];
    	IfxPort_setPinHigh(tbl->pin->port, tbl->pin->pinIndex); // P33.0 = 0
    }

    initStm0();

    /* Enable the global interrupts of this CPU */
    IfxCpu_enableInterrupts();

    /* Demo init */

    Ifx_Lwip_Config config;

    IP4_ADDR(&config.ipAddr, 192, 168, 7, 123);
    IP4_ADDR(&config.netMask, 255, 255, 255, 0);
    IP4_ADDR(&config.gateway, 192, 168, 7, 6);
    MAC_ADDR(&config.ethAddr, 0x00, 0x20, 0x30, 0x40, 0x50, 0x60);
#if 1
    Ifx_Lwip_init(&config);
#else
    EthDemo_init();
    while (!IfxEth_Phy_Pef7071_link())
    {}
    EthDemo_run();
#endif

    addr.addr8[3] = 6;
    addr.addr8[2] = 7;
    addr.addr8[1] = 168;
    addr.addr8[0] = 192;
#if 1
    udp = udp_new();
    ethRam = IfxEth_waitTransmitBuffer(&Ifx_g_Eth);
#endif

    /* background endless loop */
    total = 0;
    while (TRUE)
    {
        Ifx_Lwip_pollTimerFlags();
        Ifx_Lwip_pollReceiveFlags();
        if (total <= 10000) {
        	total++;
			p = (pbuf_t *)memp_malloc(MEMP_PBUF);
			p->payload = LWIP_MEM_ALIGN((void *)((u8_t *)ethRam));
			p->len = 100;
			p->tot_len = p->len;
			p->next = NULL;
			p->ref = 1;
			p->type = PBUF_REF;
			udp_sendto_if(udp, p, &addr, 5001, Ifx_Lwip_getNetIf());
			pbuf_free(p);
	        IfxPort_togglePin(&MODULE_P33, 6); // P33.0 = 0
        }
        REGRESSION_RUN_STOP_PASS;

    }

    return 0;
}


/** \} */
