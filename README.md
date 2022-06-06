# ESP32 - FreeRTOS DualCore Interrupt 
## 目錄
 - [環境配置](#環境配置)
 - [設計動機](#設計動機)
 - [esp32中斷步驟](#esp32中斷步驟)
 - [專案介紹](#專案介紹)
 - [Demo](#Demo)
## 環境配置
* ubuntu平臺下面安裝VScoode。
* 在VScode安裝Espressif IDF，方便建立esp32的開發環境：
https://blog.csdn.net/weixin_45652444/article/details/118728136
## 設計動機
因為上了羅習五教授的系統軟體設計，所以對IO Interrupt的處理步驟已經熟悉，想說可以藉著期末報告自己去實現在esp32的嵌入式系統下完成dual core的IO Interrupt，本專案利用Espressif IDF和FreeRTOS函數庫來實做完成。
## esp32中斷步驟
1. 安裝/建立中斷服務。
2. 創建一個靜態的IRAM_ATTR 中斷服務函數。
> 原因是GPIO的中斷在IRAM中工作。這樣的好處是在flash禁用的情況下也可以響應中斷。且速度更快，對於這種頻繁觸發的中斷是有利的。但是這個中斷也因此無法使用printf印出工作，需要轉入其他Task中執行。
3. 為某個GPIO口設置中斷服務程式 Handler（interrupt service routine Handler）。
4. 在設置的中断服务程序 Handler函數中，ISR 程序要保持簡短，不能執行耗時的工作。
## 專案介紹
本專案在core 0去做count的計數，並可以透過monitor去觀察，而core 1則是負責將button作為GPIO Interrupt藉由按按鈕的方式完成中斷，而ISR處理函數完成會讓core 0繼續執行。

新增一個名為dual_core_interrupt的專案，主程式是在main資料夾中的main.c，程式碼說明如下：
1. 初始化所有必要的標頭，如下：

```c=
#include <stdio.h>

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
```
2. 定義GPIO的pin腳和config還有初始化dual core上的task handler
```c=
#define ESP_INTR_FLAG_DEFAULT 0
#define CONFIG_LED_PIN 27
#define CONFIG_BUTTON_PIN 0

TaskHandle_t myButtonTaskHandle = NULL; //core1 上的task hanlder
TaskHandle_t myPrintTaskHandle = NULL; //core0 上的task hanlder
```
3. 創建一個靜態的IRAM_R函數作為中斷服務處理程式。即按下button，則會去call interrupt service routine或稱作interrupt handler（對應gpio interrupt)
``` c=
void IRAM_ATTR button_isr_handler(void* arg) {
    BaseType_t checkIfYieldRequired; //若架構為 32位元則定義為32位元
    checkIfYieldRequired = xTaskResumeFromISR(myButtonTaskHandle);//中斷處理函數中恢復myButtonTaskHandle task
    portYIELD_FROM_ISR(checkIfYieldRequired);
    //普通task使用taskYIELD()強制任務切換；中斷服務程序使用portYIELD_FROM_ISR()強制task切換
}
```
4. core1 上按下button產生interrupt
``` c=
void button_task(void* arg) {
	bool flag = false;
	// infinite loop
	while (1){
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
```
5. core0 上負責執行count＋＋
```c=
void myprint_task(void *arg) {
    int count = 0;
    while(1){
        vTaskDelay(1000*configTICK_RATE_HZ/1000);
        printf("CoreID : %d , count : %d\r\n",xPortGetCoreID(),count++);
    }
}
```
6. 接著定義主程式中的app_main()函數，設置 button and led pins as GPIO pins和將button gpio設為input&&led gpio設為output，再將按鈕設置為啟用中斷
```c=
void app_main()
{
	gpio_pad_select_gpio(CONFIG_BUTTON_PIN);
	gpio_pad_select_gpio(CONFIG_LED_PIN);
	
	gpio_set_direction(CONFIG_BUTTON_PIN, GPIO_MODE_INPUT);
        gpio_set_direction(CONFIG_LED_PIN, GPIO_MODE_OUTPUT);
	
	gpio_set_intr_type(CONFIG_BUTTON_PIN, GPIO_INTR_NEGEDGE);
	
        printf("( If LED is blinking means have GPIO interrupt )\r\nPlease click the button to start project!\r\n");
```
7. 利用xTaskCreatePinnedToCore去實現multicore來create task，當使用vTaskStartScheduler() 來啟動排程器決定讓哪個 task 開始執行。當 vTaskStartScheduler() 被呼叫時，會先建立一個 idle task，這個 task 是為了確保 CPU 在任一時間至少有一個 task 可以執行而在 vTaskStartScheduler() 被呼叫時自動建立的 user task，idle task 的 priority 為 0 (lowest)，目的是為了確保當有其他 user task 進入 ready list 時可以馬上被執行。
```c=
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
```
8. 允許每個GPIO註冊中斷處理程序
```c=
        gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);

        gpio_isr_handler_add(CONFIG_BUTTON_PIN, button_isr_handler, NULL);
}
```
### Demo
當按下button時會中斷計數，且可利用monitor或是led去監控有無發生中斷。
https://drive.google.com/file/d/1B2ghnNj0zjGL7SEb3q3oNo8bDdvevUeT/view?usp=sharing
