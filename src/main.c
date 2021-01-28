#include "xil_printf.h"
#include "net.h"

TaskHandle_t tcp_task_handle;

void tcp_task( void * pvParameters )
{

	xil_printf("Hola desde tcp_task\n\r");

	char msg[] = "Hola desde tcp_task\n\r";


	for(;;)
	{
		while( !tcp_is_connected() );
		net_send(msg, strlen(msg), 0);
		vTaskDelay(1000/ portTICK_PERIOD_MS);
	}

}


int main()
{
	tcp_client_initialize();

	int err;
	err = xTaskCreate(tcp_task, (const char *)"_tcp_thread", 1024, NULL, DEFAULT_THREAD_PRIO, &tcp_task_handle);
	if (pdFAIL == err)
	{
		xil_printf("Error creating TCP TASK. System stall");
		while(1);
	}

	vTaskStartScheduler();

	while(1);
	return 0;
}
