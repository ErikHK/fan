
#include <stdint.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "sdkconfig.h"
#include "i2s_example_pins.h"
#include <math.h>

#include <esp_adc/adc_oneshot.h>
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#include "driver/gptimer.h"

#include "esp_timer.h"

#include "sounds.h"

#define ALT_BUTTON_PIN 13
#define CTRL_BUTTON_PIN 14

gptimer_handle_t gptimer = NULL;
QueueHandle_t StateQueue;
bool AlarmState = false;

int16_t output_buffer[16384];


static uint16_t i = 0;
static float iff = 0;

static uint16_t i2 = 0;
static uint16_t i3 = 0;


static int16_t vol = 0;
static int16_t targetvol = 0;
static uint8_t volcounter = 0;
static int16_t volsum = 0;


static uint16_t sweepvol = 65535>>1;
static int16_t freq = 163;
static int16_t freqoffset = 0;
static int16_t mode = 0;
static int16_t ontime = 30000;

static int32_t freqsum = 0;
static uint8_t freqcounter = 0;
static int16_t targetfreq = 0;

static uint16_t tremoloindex = 0;
static int16_t tremolofreq = 10;

static float freq_q = 0;
static float pluss = 0;

static uint8_t alt_pressed = 0;
static uint8_t ctrl_pressed = 0;

static uint32_t phaseAcc = 0;

#define SAMPLE_RATE     (44100)

#define ADC_UNIT      ADC_UNIT_1        // ADC1
#define ADC_BITWIDTH  ADC_BITWIDTH_12   // 12-bit resolution (0-4095)
#define ADC_ATTEN     ADC_ATTEN_DB_12   // ~3.3V full-scale voltage

static int adc_value = 0;

adc_oneshot_unit_handle_t adc_handle;


/* Set 1 to allocate rx & tx channels in duplex mode on a same I2S controller, they will share the BCLK and WS signal
 * Set 0 to allocate rx & tx channels in simplex mode, these two channels will be totally separated,
 * Specifically, due to the hardware limitation, the simplex rx & tx channels can't be registered on the same controllers on ESP32 and ESP32-S2,
 * and ESP32-S2 has only one I2S controller, so it can't allocate two simplex channels */

#define EXAMPLE_STD_BCLK_IO1        GPIO_NUM_26     // I2S bit clock io number
#define EXAMPLE_STD_WS_IO1          GPIO_NUM_25      // I2S word select io number
#define EXAMPLE_STD_DOUT_IO1        GPIO_NUM_22     // I2S data out io number
#define EXAMPLE_STD_DIN_IO1         GPIO_NUM_22     // I2S data in io number

static i2s_chan_handle_t                tx_chan;        // I2S tx channel handler

static void write_sound()
{

    for(int16_t ii=0;ii < 256; ii++)
    {
        
        int32_t tmp = 0;

        if(mode < 1)
        {
            tmp = (vol * violin[(int32_t) iff] );
            iff += (pluss/500.0f);

            tmp = tmp >> 11;

            if(iff > 128977)
                iff = 0;


        }
        else if(mode < 2)
        {
            tmp = (vol * miau[(int16_t) iff] );
            iff += (pluss/200.0f);

            tmp = tmp >> 11;

            if(iff > 50000)
                iff = 0;

        }
        //phat saw
        else if(mode < 3)
        {
            tmp = (vol * saw[i] + vol * saw[i2]/2 );
            i += (uint16_t) (pluss);
            i2 += (uint16_t) (pluss*1.5);

            tmp = tmp >> 11;
        }
        //soft square
        else if(mode < 4)
        {
            //float vajsing = 1 + (sine65536[tremoloindex])/65536.0f/3.0f;

            tmp = vol * ( softsquare[i] );
            i += (uint16_t) (pluss);
            //i += 200;

            tmp = tmp >> 11;

            //tremoloindex += 1;
        }
        else if(mode < 5)
        {
            tmp = vol * (i > ontime)*16000;
            //tmp += vol * sine65536[tremoloindex];

            i += (uint16_t) (pluss);

            tmp = tmp >> 11;

            //tremoloindex += 2;
        }
        else if(mode < 6)
        {
            tmp = vol * ( saw[i] );
            i += (uint16_t) (pluss);
            //i += 200;

            tmp = tmp >> 11;

        }
        else if(mode < 7)
        {
            tmp = (vol * miau[(int32_t) iff] );
            iff += (pluss/200.0f);

            tmp = tmp >> 11;

            if(iff > 117888)
                iff = 0;

        }


        
        output_buffer[ii] =  (int16_t) tmp;
        

        if(vol < 150)
            iff = 0;

    }


    i2s_channel_write(tx_chan, &output_buffer, 256*2, NULL, 1000);

}


static void vol_envelope(void *userData)
{
    while(1)
    {
        /*
        if(targetvol > vol)
            vol++;
        else 
            vol--;

            */

        //if(targetvol < 3)
        //    vol = 0;


        vTaskDelay(pdMS_TO_TICKS(10));
    }

}

static void tremolo_task(void *userData)
{
    while(1)
    {

        //tremoloindex += tremolofreq;



        vTaskDelay(pdMS_TO_TICKS(10));
    }

}


void read_buttons(void *userData)
{
    //while(1)
    //{
        if(gpio_get_level(CTRL_BUTTON_PIN))
            ctrl_pressed = 1;
        else
            ctrl_pressed = 0;


        if(gpio_get_level(ALT_BUTTON_PIN))
            alt_pressed = 1;
        else
            alt_pressed = 0;

        
        //min 750 max 2800!
        adc_oneshot_read(adc_handle, ADC_CHANNEL_1, &adc_value);


        //targetvol = (adc_value - 740);

        int16_t tmpvol = (adc_value - 740);

        if(tmpvol < 0)
            tmpvol = 0;

        if(tmpvol >= 2047)
            tmpvol = 2047;
        

        volsum += tmpvol;

        volcounter++;

        if(volcounter == 15)
        {
            targetvol = volsum >> 4;
            volcounter = 0;
            volsum = 0;
        }

        
        adc_oneshot_read(adc_handle, ADC_CHANNEL_0, &adc_value);

        if(adc_value > 40)
            //freqsum += adc_value;
            freq = adc_value;
        
        //freqcounter++;

        //if(freqcounter == 3)
        //{
        //    freq = freqsum >> 2;
        //    freqcounter = 0;
        //    freqsum = 0;
        //}


        //freq = 163;

        int16_t freqoffsetq = freqoffset/100;

        float freqoffsetf = freqoffsetq / 12.0f;

        freq_q = (float) (freq + 100) / 1860.0f;

        pluss = 45 * pow(2.0f, freq_q + freqoffsetf);

        //printf("%d\n", vol);


        adc_oneshot_read(adc_handle, ADC_CHANNEL_2, &adc_value);

        if(ctrl_pressed)
            tremolofreq = adc_value>>2;
        else
            freqoffset = adc_value;


        adc_oneshot_read(adc_handle, ADC_CHANNEL_3, &adc_value);

        if(!ctrl_pressed)
            mode = adc_value >> 9;


        adc_oneshot_read(adc_handle, ADC_CHANNEL_4, &adc_value);

        if(ctrl_pressed)
            ontime = adc_value << 3;

        printf("ontime %d\n", ontime);
        
        //vTaskDelay(pdMS_TO_TICKS(10)); // Delay ms
    //}
}



static void audio_task(void *userData)
{
    
    while(1) {
        write_sound();




        //vTaskDelay(pdMS_TO_TICKS(20)); // Delay ms
    }
}


void timer_callback(void *param)
{
    //targetvol += 100;

    if(targetvol > vol)
            vol+=25;
        else 
            vol-=25;
}



void app_main(void)
{


    //button read timer
    
    const esp_timer_create_args_t my_timer_args2 = 
    {
        .callback = &read_buttons,
        .name = "Timer Interrupt"
    };

    esp_timer_handle_t timer_handler2;
    esp_timer_create(&my_timer_args2, &timer_handler2);
    esp_timer_start_periodic(timer_handler2, 5000);  // One Second = 1000000 micro second




    const esp_timer_create_args_t my_timer_args = 
    {
        .callback = &timer_callback,
        .name = "Timer Interrupt"
    };

    esp_timer_handle_t timer_handler;
    esp_timer_create(&my_timer_args, &timer_handler);
    esp_timer_start_periodic(timer_handler, 1000);  // One Second = 1000000 micro second



    // Initialize ADC Oneshot Mode Driver on the ADC Unit
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT,

    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));

    // Configure ADC channel
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH,
        .atten = ADC_ATTEN_DB_12,
    };

    adc_oneshot_chan_cfg_t config1 = {
        .bitwidth = ADC_BITWIDTH,
        .atten = ADC_ATTEN,
    };

    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_0, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_1, &config1));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_2, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_3, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_4, &config));


    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_chan, NULL));

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = EXAMPLE_STD_BCLK_IO1,
            .ws = EXAMPLE_STD_WS_IO1,
            .dout = EXAMPLE_STD_DOUT_IO1,
            .din = EXAMPLE_STD_DIN_IO1,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };




    gpio_config_t io_conf = {
    .pin_bit_mask = (1ULL << CTRL_BUTTON_PIN),   // Select GPIO 14
    .mode = GPIO_MODE_INPUT_OUTPUT,                  // Set as input
    .pull_up_en = GPIO_PULLUP_DISABLE,     // Enable internal pull-up
    .pull_down_en = GPIO_PULLDOWN_ENABLE, // Disable pull-down
    .intr_type = GPIO_INTR_DISABLE        // Disable interrupts
    };

    gpio_config(&io_conf);

    
    gpio_config_t io_conf2 = {
    .pin_bit_mask = (1ULL << ALT_BUTTON_PIN),   // Select GPIO 14
    .mode = GPIO_MODE_INPUT_OUTPUT,                  // Set as input
    .pull_up_en = GPIO_PULLUP_DISABLE,     // Enable internal pull-up
    .pull_down_en = GPIO_PULLDOWN_ENABLE, // Disable pull-down
    .intr_type = GPIO_INTR_DISABLE        // Disable interrupts
    };

    gpio_config(&io_conf2);



    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_chan, &std_cfg));

    ESP_ERROR_CHECK(i2s_channel_enable(tx_chan));


    //xTaskCreate(audio_task, "audio", 4096, NULL, configMAX_PRIORITIES - 1, NULL);
    xTaskCreate(audio_task, "audio", 4096, NULL, 2 | portPRIVILEGE_BIT, NULL);
    //xTaskCreate(read_buttons, "read_buttons", 4096, NULL, 5, NULL);
    //xTaskCreate(tremolo_task, "tremolo", 4096, NULL, 5, NULL);
    //xTaskCreate(vol_envelope, "volume", 4096, NULL, ( 1 | portPRIVILEGE_BIT ), NULL);
}
