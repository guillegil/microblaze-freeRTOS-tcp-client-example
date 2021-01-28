#include "net.h"

#include "lwipopts.h"
#include "xlwipconfig.h"
#include "lwip/tcp.h"
#include "lwip/ip_addr.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "xil_printf.h"
//#include <sleep.h>

static int complete_nw_thread;
static sys_thread_t main_thread_handle;
static int socket_fd;

#if LWIP_IPV6==1
	static struct sockaddr_in6 address;
#else
	static struct sockaddr_in address;
#endif /* LWIP_IPV6 */

void main_thread(void *p);
static void start_application();

#define THREAD_STACKSIZE 1024

struct netif server_netif;

#if LWIP_IPV6==1
static void print_ipv6(char *msg, ip_addr_t *ip)
{
	print(msg);
	xil_printf(" %s\n\r", inet6_ntoa(*ip));
}
#else

static void print_ip(char *msg, ip_addr_t *ip)
{
	xil_printf(msg);
	xil_printf("%d.%d.%d.%d\n\r", ip4_addr1(ip), ip4_addr2(ip), ip4_addr3(ip), ip4_addr4(ip));
}

static void print_ip_settings(ip_addr_t *ip, ip_addr_t *mask, ip_addr_t *gw)
{
	print_ip("Board IP:       ", ip);
	print_ip("Netmask :       ", mask);
	print_ip("Gateway :       ", gw);
}

static void assign_default_ip(ip_addr_t *ip, ip_addr_t *mask, ip_addr_t *gw)
{
	int err;

	xil_printf("Configuring default IP %s \r\n", DEFAULT_IP_ADDRESS);

	err = inet_aton(DEFAULT_IP_ADDRESS, ip);
	if(!err)
	{
		xil_printf("Invalid default IP address: %d\r\n", err);
	}

	err = inet_aton(DEFAULT_IP_MASK, mask);
	if(!err)
	{
		xil_printf("Invalid default IP MASK: %d\r\n", err);
	}

	err = inet_aton(DEFAULT_GW_ADDRESS, gw);
	if(!err)
	{
		xil_printf("Invalid default gateway address: %d\r\n", err);
	}
}
#endif /* LWIP_IPV6 */


static void start_application()
{
	int err = 0;

	/* set up address to connect to */
	memset(&address, 0, sizeof(address));

#if LWIP_IPV6==1
	if ((sock = socket(AF_INET6, SOCK_STREAM, 0)) < 0) {
	        xil_printf("TCP Client: Error in creating Socket\r\n");
	        return;
	}
	address.sin6_family = AF_INET6;
	address.sin6_port = htons(TCP_CONN_PORT);
	inet6_aton(TCP_SERVER_IPV6_ADDRESS, &address.sin6_addr.s6_addr);
#else

	socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if ( socket_fd < 0 )
	{
		xil_printf("TCP Client: Error in creating Socket\r\n");
		while(1);
	}

	address.sin_family = AF_INET;
	address.sin_port = htons(TCP_CONN_PORT);
	address.sin_addr.s_addr = inet_addr(TCP_SERVER_IP_ADDRESS);
#endif /* LWIP_IPV6 */

	err = connect(socket_fd, (struct sockaddr*)&address, sizeof(address));
	if ( err < 0 )
	{
		xil_printf("TCP Client: Error on tcp_connect %d\r\n", err);
		close(socket_fd);
		while(1);
	}

	_tcp_is_connected = 1;
}


void network_thread(void *p)
{
#if ((LWIP_IPV6==0) && (LWIP_DHCP==1))
	int mscnt = 0;
#endif
	/* the mac address of the board. this should be unique per board */
	u8_t mac_ethernet_address[] = { 0x00, 0x0a, 0x35, 0x00, 0x01, 0x02 };

	xil_printf("\n\r\n\r");
	xil_printf("------lwIP Socket Mode TCP Client Application------\r\n");

	/* Add network interface to the netif_list, and set it as default */
	if (!xemac_add(&server_netif, NULL, NULL, NULL, mac_ethernet_address, PLATFORM_EMAC_BASEADDR))
	{
		xil_printf("Error adding N/W interface\r\n");
		return;
	}

#if LWIP_IPV6==1
	server_netif.ip6_autoconfig_enabled = 1;
	netif_create_ip6_linklocal_address(&server_netif, 1);
	netif_ip6_addr_set_state(&server_netif, 0, IP6_ADDR_VALID);
	print_ipv6("\n\rlink local IPv6 address is:",&server_netif.ip6_addr[0]);
#endif /* LWIP_IPV6 */

	netif_set_default(&server_netif);

	/* specify that the network if is up */
	netif_set_up(&server_netif);

	/* start packet receive thread - required for lwIP operation */
	sys_thread_new("xemacif_input_thread", (void(*)(void*))xemacif_input_thread, &server_netif, THREAD_STACKSIZE, DEFAULT_THREAD_PRIO);

	complete_nw_thread = 1;

	/* Resume the main thread; auto-negotiation is completed */
	vTaskResume(main_thread_handle);

#if ((LWIP_IPV6==0) && (LWIP_DHCP==1))
	dhcp_start(&server_netif);
	while (1)
	{
		vTaskDelay(DHCP_FINE_TIMER_MSECS / portTICK_RATE_MS);
		dhcp_fine_tmr();
		mscnt += DHCP_FINE_TIMER_MSECS;
		if (mscnt >= DHCP_COARSE_TIMER_SECS*1000)
		{
			dhcp_coarse_tmr();
			mscnt = 0;
		}
	}
#else
	vTaskDelete(NULL);
#endif
	return;
}

void main_thread(void *p)
{

#if ((LWIP_IPV6==0) && (LWIP_DHCP==1))
	int mscnt = 0;
#endif

#ifdef XPS_BOARD_ZCU102
	IicPhyReset();
#endif
	/* initialize lwIP before calling sys_thread_new */
	lwip_init();

	/* any thread using lwIP should be created using sys_thread_new */
	sys_thread_new("nw_thread", network_thread, NULL, THREAD_STACKSIZE, DEFAULT_THREAD_PRIO);

	/* Suspend Task until auto-negotiation is completed */
	if (!complete_nw_thread)
	{
		vTaskSuspend(NULL);
	}

#if LWIP_IPV6==0
#if LWIP_DHCP==1
	while (1)
	{
		vTaskDelay(DHCP_FINE_TIMER_MSECS / portTICK_RATE_MS);
		if (server_netif.ip_addr.addr)
		{
			xil_printf("DHCP request success\r\n");
			break;
		}

		mscnt += DHCP_FINE_TIMER_MSECS;
		if (mscnt >= 10000)
		{
			xil_printf("ERROR: DHCP request timed out\r\n");
			assign_default_ip(&(server_netif.ip_addr), &(server_netif.netmask), &(server_netif.gw));
			break;
		}
	}

#else
	assign_default_ip(&(server_netif.ip_addr), &(server_netif.netmask),
								&(server_netif.gw));
#endif

	print_ip_settings(&(server_netif.ip_addr), &(server_netif.netmask), &(server_netif.gw));
#endif /* LWIP_IPV6 */

	xil_printf("\r\n");
	/* start the application*/

	start_application();
	while(1);

	return;
}



void tcp_client_initialize()
{
	_tcp_is_connected = 0;
	main_thread_handle = sys_thread_new("main_thread", main_thread, 0, THREAD_STACKSIZE + 256,DEFAULT_THREAD_PRIO);
}


void net_send(const void *data, size_t size, int flags)
{
	send(socket_fd, data, size, flags);
}

int tcp_is_connected()
{
	return _tcp_is_connected;
}
