/******************************************************************************
   thingspeak-esp32-dht11.c - A example thingspeak client, logging temperature 
                              and humidity.
							 
   Author: Mike Field <hamster@snap.net.nz>
							 
   Largely adapted from the ESP-IDF OpenSSL client example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*******************************************************************************
*  Configure like the rest of the ESP-IDF examples, with "make menuconfig" 
******************************************************************************/

#include "dht11.h"

const static char *TAG = "DHT11";

int temp_x10 = 255;
int humidity = 60;
// RMT receiver initialization
static void dht11_rmt_rx_init(int gpio_pin, int channel)
{
	const int RMT_CLK_DIV            = 80;     /*!< RMT counter clock divider */
	const int RMT_TICK_10_US         = (80000000/RMT_CLK_DIV/100000);   /*!< RMT counter value for 10 us.(Source clock is APB clock) */
	const int  rmt_item32_tIMEOUT_US = 1000;   /*!< RMT receiver timeout value(us) */

    rmt_config_t rmt_rx;
    rmt_rx.gpio_num                      = gpio_pin;
    rmt_rx.channel                       = channel;
    rmt_rx.clk_div                       = RMT_CLK_DIV;
    rmt_rx.mem_block_num                 = 1;
    rmt_rx.rmt_mode                      = RMT_MODE_RX;
    rmt_rx.rx_config.filter_en           = false;
    rmt_rx.rx_config.filter_ticks_thresh = 100;
    rmt_rx.rx_config.idle_threshold      = rmt_item32_tIMEOUT_US / 10 * (RMT_TICK_10_US);
    rmt_config(&rmt_rx);
    rmt_driver_install(rmt_rx.channel, 1000, 0);
}

// Processing the pulse data into temp and humidity

static int parse_items(rmt_item32_t* item, int item_num, int *humidity, int *temp_x10)
{
	int i=0;
	unsigned rh = 0, temp = 0, checksum = 0;

	// Check we have enough pulses
    if(item_num < 42)  return 0;

	// Skip the start of transmission pulse
	item++;  

    // Extract the humidity data 	
	for(i = 0; i < 16; i++, item++) 
	    rh = (rh <<1) + (item->duration1 < 35 ? 0 : 1);

	// Extract the temperature data
	for(i = 0; i < 16; i++, item++) 
	    temp = (temp <<1) + (item->duration1 < 35 ? 0 : 1);

	// Extract the checksum
	for(i = 0; i < 8; i++, item++) 
	    checksum = (checksum <<1) + (item->duration1 < 35 ? 0 : 1);
	
	// Check the checksum
	if((((temp>>8) + temp + (rh>>8) + rh)&0xFF) != checksum) {
		printf("Checksum failure %4X %4X %2X\n", temp, rh, checksum);
       return 0;   
    }

	// Store into return values
    *humidity = rh>>8;
	*temp_x10 = (temp>>8)*10+(temp&0xFF);
    return 1;
}

// Use the RMT receiver to get the DHT11 data
static int dht11_rmt_rx(int gpio_pin, int rmt_channel, 
                        int *humidity, int *temp_x10)
{
    RingbufHandle_t rb = NULL;
    size_t rx_size = 0;
	rmt_item32_t* item;
	int rtn = 0;
	
	//get RMT RX ringbuffer
    rmt_get_ringbuf_handle(rmt_channel, &rb);
    if(!rb) 
		return 0;
	// Send the 20ms pulse to kick the DHT11 into life
	gpio_set_level( gpio_pin, 1 );
	gpio_set_direction( gpio_pin, GPIO_MODE_OUTPUT );
	ets_delay_us( 1000 );
	gpio_set_level( gpio_pin, 0 );
	ets_delay_us( 20000 );

	// Bring rmt_rx_start & rmt_rx_stop into cache
    rmt_rx_start(rmt_channel, 1);
    rmt_rx_stop(rmt_channel);

	// Now get the sensor to send the data
	gpio_set_level( gpio_pin, 1 );
	gpio_set_direction( gpio_pin, GPIO_MODE_INPUT );
	
	// Start the RMT receiver for the data this time
    rmt_rx_start(rmt_channel, 1);
    
	// Pull the data from the ring buffer
	item = (rmt_item32_t*) xRingbufferReceive(rb, &rx_size, 2);
	if(item != NULL) {
		int n;
		n = rx_size / 4 - 0;
		//parse data value from ringbuffer.
		rtn = parse_items(item, n, humidity, temp_x10);
		//after parsing the data, return spaces to ringbuffer.
		vRingbufferReturnItem(rb, (void*) item);
	}
    rmt_rx_stop(rmt_channel);

	return rtn;
}
static void getData(void * parm)
{
    while(1){
        if(dht11_rmt_rx(15, 0, &humidity, &temp_x10)) {
        ESP_LOGI(TAG, "Temperature & humidity read %i.%iC & %i%%\n", temp_x10/10, temp_x10%10, humidity);
        } else {
            ESP_LOGI(TAG, "Sensor failure - retrying\n");
            vTaskDelay(pdMS_TO_TICKS(250));
            if(dht11_rmt_rx(15, 0, &humidity, &temp_x10)) {
                ESP_LOGI(TAG, "Temperature & humidity read %i.%iC & %i%%\n", temp_x10/10, temp_x10%10, humidity);
            } else {
            ESP_LOGI(TAG, "Sensor failure\n");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
void dht11_enable(void)
{
    dht11_rmt_rx_init(15,0);
	vTaskDelay(pdMS_TO_TICKS(500));
    xTaskCreate(getData,"getdata",1024*4,NULL,0,NULL);
}
float get_temp()
{
    return ((float)temp_x10)/10;
}
int get_hum()
{
    return humidity;
}