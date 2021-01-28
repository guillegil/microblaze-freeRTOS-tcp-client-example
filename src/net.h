#ifndef SRC_NET_H_
#define SRC_NET_H_


#include <unistd.h>
#include "netif/xadapter.h"
#include "platform_config.h"
#include "xil_printf.h"
#include "lwip/init.h"
#include "lwip/inet.h"

#include "FreeRTOS.h"
#include "task.h"

#if LWIP_IPV6==1
#include "lwip/ip6_addr.h"
#include "lwip/ip6.h"
#else

#if LWIP_DHCP==1
#include "lwip/dhcp.h"
extern volatile int dhcp_timoutcntr;
err_t dhcp_start(struct netif *netif);
#endif
#define DEFAULT_IP_ADDRESS	"192.168.1.10"
#define DEFAULT_IP_MASK		"255.255.255.0"
#define DEFAULT_GW_ADDRESS	"192.168.1.1"
#endif /* LWIP_IPV6 */

#ifdef XPS_BOARD_ZCU102
#ifdef XPAR_XIICPS_0_DEVICE_ID
int IicPhyReset(void);
#endif
#endif

int _tcp_is_connected;


#define INTERIM_REPORT_INTERVAL 5

#define TCP_CLIENT_THREAD_STACKSIZE 2048

/* Client port to connect */
#define TCP_CONN_PORT 5004

/* time in seconds to transmit packets */
#define TCP_TIME_INTERVAL 300

#if LWIP_IPV6==1
/* Server to connect with */
#define TCP_SERVER_IPV6_ADDRESS "fe80::6600:6aff:fe71:fde6"
#else
/* Server to connect with */
#define TCP_SERVER_IP_ADDRESS "192.168.0.103"
#endif

void tcp_client_initialize();
void net_send(const void *data, size_t size, int flags);
int tcp_is_connected();

#endif /* SRC_NET_H_ */
