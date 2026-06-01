/*
 * udp_server.c
 *
 *  Created on: Jul 9, 2021
 *      Author: user
 */
#include <pbuf.h>
#include <udp.h>
#include <tcp.h>
#include <string.h>
#include <stdio.h>
#include "lwip/mem.h"
#include "lwip/memp.h"
#include "lwip/tcp.h"
#include "lwip/udp.h"
#include "netif/etharp.h"
#include "lwip/dhcp.h"
#include "ethernetif.h"
#include "main.h"
#include <stdio.h>

#define LEDG_TICKING_INTERVAL_MS 100 // Интервал мигания зелёного светодиода
#define LEDG_TICKING_TIMEOUT_MS 400 // Таймаут для мигания светодиода. После того, как пройдёт заданное количество миллисекунд после последнего принятого UDP пакета, мигание зелёного светодиода прекратится

void udp_server_receive_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port);
short TX_ETH[160];
short RX_ETH[160];
extern short FROM_CONT[];
extern short TO_CONT[];
typedef enum {false, true} bool;
volatile uint32_t sysTickCount = 0;
volatile bool LED_ticking_flag = false;
volatile bool UDP_receiving_flag = false;
uint32_t TCPTimer = 0;
uint32_t ARPTimer = 0;
uint32_t LEDG_ticking_timer = 0;
uint32_t LEDG_ticking_timeout = 0;

void udp_server_init(uint32_t sys_tick_count) {
	sysTickCount = sys_tick_count;
	struct udp_pcb *upcb;
	err_t err;
	upcb = udp_new();
	if (upcb) {
		err = udp_bind(upcb, IPADDR_ANY, 10003);
		if (err == ERR_OK) {
			udp_recv(upcb, udp_server_receive_callback, NULL);

		}
	}
}

void udp_server_receive_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port) {
	LED_ticking_flag = true;
	LEDG_ticking_timeout = sysTickCount;
	uint8_t i;
	pbuf_copy_partial(p, (char*)RX_ETH, 320, 0);
	pbuf_free(p);
	p = pbuf_alloc(PBUF_TRANSPORT, 320, PBUF_POOL);

	for(i=0;i<158;i++){
		TX_ETH[i] = FROM_CONT[i];
		TO_CONT[i] = RX_ETH[i];
	}
	if (p != NULL) {
		pbuf_take(p, (char*)TX_ETH, 320);
	}
	udp_sendto(pcb, p, addr, 10004);
	pbuf_free(p);
}

void periodic_handler() {

//#if LWIP_TCP
  /* TCP periodic process every 250 ms */
//  if (systickcount - TCPTimer >= 250) {
//    TCPTimer =  systickcount;
//    tcp_tmr();
//  }

//#endif

  /* ARP periodic process every 1s */
	if (sysTickCount < ARPTimer)
		ARPTimer =  sysTickCount;
  if ((sysTickCount - ARPTimer) >= ARP_TMR_INTERVAL) {
    ARPTimer = sysTickCount;
    etharp_tmr();
  }

  if (LED_ticking_flag) {
	  if (sysTickCount < LEDG_ticking_timer)
	  	  LEDG_ticking_timer = sysTickCount;
	  if ((sysTickCount - LEDG_ticking_timer) >= LEDG_TICKING_INTERVAL_MS) {
	  	  LEDG_ticking_timer = sysTickCount;
	  	  HAL_GPIO_TogglePin(LEDG_GPIO_Port, LEDG_Pin);
	  }

	  if (sysTickCount < LEDG_ticking_timeout)
		  LEDG_ticking_timeout = sysTickCount;
	  if ((sysTickCount - LEDG_ticking_timeout) >= LEDG_TICKING_TIMEOUT_MS) {
		  LEDG_ticking_timeout = sysTickCount;
		  LED_ticking_flag = false;
	  }
  } else
	  HAL_GPIO_WritePin(LEDG_GPIO_Port, LEDG_Pin, GPIO_PIN_SET);
}



