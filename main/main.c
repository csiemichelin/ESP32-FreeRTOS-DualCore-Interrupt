#include <stdio.h>

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define ESP_INTR_FLAG_DEFAULT 0
#define CONFIG_LED_PIN 27
#define CONFIG_BUTTON_PIN 0

TaskHandle_t myButtonTaskHandle = NULL; //core1 上的task hanlder
TaskHandle_t myPrintTaskHandle = NULL; //core0 上的task hanlder

// 當按下button，則會去call interrupt service routine處理函數或較interrupt handler（對應gpio interrupt)
void IRAM_ATTR button_isr_handler(void* arg) {
    BaseType_t checkIfYieldRequired; //若架構為 32位元則定義為32位元
    checkIfYieldRequired = xTaskResumeFromISR(myButtonTaskHandle);//中斷處理函數中恢復myButtonTaskHandle task
    portYIELD_FROM_ISR(checkIfYieldRequired);
    //普通task使用taskYIELD()強制任務切換；中斷服務程序使用portYIELD_FROM_ISR()強制task切換
}

// core1 上按下button製造interrupt
void button_task(void* arg) {
	bool flag = false;
	// infinite loop
	while (1)
    {
        vTaskSuspend(NULL);//suspended itself先讓printTask先執行null是own task
        if(flag){
            printf("CoreID : %d , Button pressed GPIO interrupt!\r\n",xPortGetCoreID());
            gpio_set_level(CONFIG_LED_PIN, 1);//閃燈代表有gpio interrupt
            vTaskDelay(10);
            gpio_set_level(CONFIG_LED_PIN, 0);
        }  
        flag = true;
    }
}

// core0 上負責執行count＋＋
void myprint_task(void *arg) {
    int count = 0;
    while(1){
        vTaskDelay(1000*configTICK_RATE_HZ/1000);
        printf("CoreID : %d , count : %d\r\n",xPortGetCoreID(),count++);
    }
}

void app_main()
{
	// 設置 button and led pins as GPIO pins
	gpio_pad_select_gpio(CONFIG_BUTTON_PIN);
	gpio_pad_select_gpio(CONFIG_LED_PIN);
	
	gpio_set_direction(CONFIG_BUTTON_PIN, GPIO_MODE_INPUT);//將button gpio設為input
    gpio_set_direction(CONFIG_LED_PIN, GPIO_MODE_OUTPUT);//將led gpio設為output
	
	// 在按鈕從 (1->0) 則啟用中斷
	gpio_set_intr_type(CONFIG_BUTTON_PIN, GPIO_INTR_NEGEDGE);
	
    printf("( If LED is blinking means have GPIO interrupt )\r\nPlease click the button to start project!\r\n");

	// 建立myprint_task並指定在核心0中執行
    xTaskCreatePinnedToCore(
            myprint_task, //本任務實際對應的Function
            "print_task", //任務名稱（自行設定）
            2048, //所需堆疊空間（常用10000）
            NULL, //輸入值
            3, //優先序：0代表最優先執行，1次之，以此類推
            &myPrintTaskHandle,  //對應的任務handle變數
            0 //指定執行核心編號（0、1或tskNO_AFFINITY：系統指定)
            );
	xTaskCreatePinnedToCore(button_task, "button_task", 2048, NULL, 10, &myButtonTaskHandle, 1);
    
    vTaskStartScheduler();//vTaskStartScheduler() 來啟動排程器決定讓哪個 task 開始執行

	// 允許每個GPIO註冊中斷處理程序
	gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
	
	// 註冊中斷
	gpio_isr_handler_add(CONFIG_BUTTON_PIN, button_isr_handler, NULL);
}