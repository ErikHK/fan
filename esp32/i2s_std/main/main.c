
#include <stdint.h>
#include <stdlib.h>

//#include "esp_midi.h"
#include "esp_log.h"


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "sdkconfig.h"
#include "i2s_example_pins.h"
#include <math.h>

#include <esp_adc/adc_oneshot.h>
#include "esp_adc/adc_continuous.h"

#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#include "driver/gptimer.h"

#include "esp_timer.h"

#include "sounds.h"

#include "movingaverage.h"

extern const uint8_t raw_sample_start[] asm("_binary_PanFlute_wav_start");
extern const uint8_t raw_sample_end[]   asm("_binary_PanFlute_wav_end");

const int16_t *raw_pcm_samples = (const int16_t *)(raw_sample_start + 44);


extern const uint8_t churchorgan_start[] asm("_binary_ChurchOrgan_wav_start");
extern const uint8_t churchorgan_end[]   asm("_binary_ChurchOrgan_wav_end");

const int16_t *churchorgan_samples = (const int16_t *)(churchorgan_start + 44);


extern const uint8_t electricpiano_start[] asm("_binary_ElectricPiano_wav_start");
extern const uint8_t electricpiano_end[]   asm("_binary_ElectricPiano_wav_end");

const int16_t *electricpiano_samples = (const int16_t *)(electricpiano_start + 44);


#define ALT_BUTTON_PIN 13
#define CTRL_BUTTON_PIN 14

gptimer_handle_t gptimer = NULL;
QueueHandle_t StateQueue;
bool AlarmState = false;

int16_t output_buffer[16384];


static uint16_t ontime = 0;
static uint16_t offtime = 0;

static uint8_t ison = 0;

//static uint16_t i = 0;
static uint32_t i = 0;

static float iff = 0;

static uint16_t i2 = 0;
static uint16_t i3 = 0;


static int16_t vol = 20;
static int32_t targetvol = 20;
static uint8_t volcounter = 0;
static int32_t volsum = 0;

static uint16_t sweepvol = 65535>>1;
static int16_t freq = 163;
static int16_t freqoffset = 0;
static int16_t mode = 3;
static int16_t dutycycle = 30000;

static int32_t freqsum = 0;
static uint8_t freqcounter = 0;
static int16_t targetfreq = 0;

static uint16_t tremoloindex = 0;
static int16_t tremolofreq = 10;
static int16_t tremoloamount = 100;

static float freq_q = 0;
static float freq_q2 = 0;
static float pluss = 200;

static int16_t freq2 = 0;
static float pluss2 = 200;


static uint8_t alt_pressed = 0;
static uint8_t ctrl_pressed = 0;

static uint32_t phaseAcc = 0;

#define SAMPLE_RATE     (32000)

#define ADC_UNIT      ADC_UNIT_1        // ADC1
#define ADC_BITWIDTH  ADC_BITWIDTH_12   // 12-bit resolution (0-4095)
#define ADC_ATTEN     ADC_ATTEN_DB_12   // ~3.3V full-scale voltage


#define EXAMPLE_READ_LEN 16
uint8_t result[EXAMPLE_READ_LEN] = {0};


static int adc_value = 0;

adc_continuous_handle_t handle = NULL;


//static adc_channel_t channel[5] = { /* ADC_CHANNEL_0, */ ADC_CHANNEL_1, ADC_CHANNEL_2, ADC_CHANNEL_3, ADC_CHANNEL_4};



static const char *TAG = "ADC_MA_4CH";

#define ADC_UNIT        ADC_UNIT_1
#define NUM_CHANNELS    4
#define WINDOW_SIZE     8
#define SAMPLE_PERIOD_MS 20

static const adc_channel_t channels[NUM_CHANNELS] = {
    ADC_CHANNEL_0,
    ADC_CHANNEL_1,
    ADC_CHANNEL_2,
    ADC_CHANNEL_3
};

// Shared context between the ADC task and the rest of your application
typedef struct {
    adc_oneshot_unit_handle_t adc_handle;
    moving_average_t filters[NUM_CHANNELS];
    int smoothed_vals[NUM_CHANNELS];
    SemaphoreHandle_t mutex;
} adc_context_t;


static adc_context_t adc_ctx;
static int filter_buffers[NUM_CHANNELS][WINDOW_SIZE];

// Thread-safe helper for other tasks to read the latest filtered value
int get_smoothed_adc_value(int channel_idx) {
    if (channel_idx < 0 || channel_idx >= NUM_CHANNELS) {
        return 0;
    }

    int val = 0;
    if (xSemaphoreTake(adc_ctx.mutex, portMAX_DELAY) == pdTRUE) {
        val = adc_ctx.smoothed_vals[channel_idx];
        xSemaphoreGive(adc_ctx.mutex);
    }
    return val;
}




// Dedicated ADC Sampling Task
static void adc_sampling_task(void *pvParameters) {
    adc_context_t *ctx = (adc_context_t *)pvParameters;
    int raw_vals[NUM_CHANNELS] = {0};

    // Use vTaskDelayUntil for consistent, drift-free sampling intervals
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t frequency = pdMS_TO_TICKS(SAMPLE_PERIOD_MS);

    while (1) {
        for (int i = 0; i < NUM_CHANNELS; i++) {
            esp_err_t ret = adc_oneshot_read(ctx->adc_handle, channels[i], &raw_vals[i]);
            if (ret == ESP_OK) {
                int avg = 0;

                if(raw_vals[i] > 80)
                    avg = moving_average_add(&ctx->filters[i], raw_vals[i]);

                // Safely update the shared output array
                if (xSemaphoreTake(ctx->mutex, portMAX_DELAY) == pdTRUE) {
                    ctx->smoothed_vals[i] = avg;
                    xSemaphoreGive(ctx->mutex);
                }
            } else {
                ESP_LOGE(TAG, "ADC Read failed on channel %d", channels[i]);
            }
        }

        ESP_LOGI(TAG, "CH0: %4d (%4d) | CH1: %4d (%4d) | CH2: %4d (%4d) | CH3: %4d (%4d)",
                 raw_vals[0], ctx->smoothed_vals[0],
                 raw_vals[1], ctx->smoothed_vals[1],
                 raw_vals[2], ctx->smoothed_vals[2],
                 raw_vals[3], ctx->smoothed_vals[3]);

        vTaskDelayUntil(&last_wake_time, frequency);
    }
}





//static adc_channel_t channel[1] = {ADC_CHANNEL_0};

//static TaskHandle_t s_task_handle;
//static const char *TAG = "EXAMPLE";


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

        if(mode == 0)
        {
            //tmp = (vol * violin[(int32_t) iff] );
            iff += (pluss/500.0f);

            tmp = tmp >> 11;

            if(iff > 128977)
                iff = 0;


        }
        else if(mode == 1)
        {
            if(vol > 10)
              tmp =  miau[(uint16_t) iff];

            iff += (pluss/200.0f);

            tmp = tmp >> 1;

            if(iff > 33000)
                iff = 0;

        }
        //phat saw
        else if(mode == 2)
        {

            //uint16_t sinestuff = (sine65536[tremoloindex] + 17000)>>7;
            //tmp = (sinestuff * (vol * saw[i] + vol * saw[i2]/2 )) >> 8;

            //vol = 60000;
            tmp = (vol * saw[i] + vol * saw[i2]/2 );
            //tmp = saw[i];

            i += (uint16_t) (pluss);
            i2 += (uint16_t) (pluss*1.5);

            tmp = tmp >> 10;
            
        }
        //soft square
        else if(mode == 3)
        {
            //float vajsing = 1 + (sine65536[tremoloindex])/65536.0f/3.0f;

            //tmp = vol * ( softsquare[i] );

            //if(ctrl_pressed)
            //    play_sample_at_pitch(tx_chan, pluss / 100.f);

            //if(ctrl_pressed)
                //tmp = vol * raw_pcm_samples[(uint16_t)iff];
            
            tmp = vol * churchorgan_samples[(uint32_t)iff];
            float fact = pluss / 300.f;
            //i += (uint16_t) (fact);
            iff += (fact);
            //i++;
            //i += 200;

            if((uint32_t)iff < 15104)
                iff = 15104;

            if((uint32_t)iff > 71862)
                iff = 0;

            tmp = tmp >> 11;

            tmp = tmp - 32760;
            
            //printf("vol:  %d\n", (int)vol);

            //tremoloindex += 1;
        }
        else if(mode == 4)
        {
            //vol = 2047;

            //printf("vol:  %d\n", (int)vol);

            tmp = vol * (i > dutycycle)*16000;
            
            //tmp += vol * sine65536[tremoloindex];

            i += (uint16_t) (pluss);
            tmp = (tmp >> 9);
         
            tmp = tmp - 32000;

            //printf("tmp:  %d\n", (int)tmp);


            //tremoloindex += 2;
        }
        else if(mode == 5)
        {
            tmp = vol * ( saw[i] );
            i += (uint16_t) (pluss);
            //i += 200;

            tmp = tmp >> 11;

        }
        else if(mode == 6)
        {
            tmp = vol * ( softsquare[i] + (softsquare[i2]>>1) );
            i += (uint16_t) (pluss);
            i2 += (uint16_t) (pluss*1.5);
            //i += 200;

            tmp = tmp >> 10;

        }
        //DRUM mode???
        else if(mode == 7)
        {
            if(ison)
            {
                if(freq < 500)
                {
                    tmp = kickdrum[i];
                    i += 1;

                    if(i > 50000)
                        i = 0;

                }
                else if(freq >= 500 && freq < 3000)
                {
                    tmp = phatsnare[i];
                    i += 1;
   
                }
                
                else
                {
                    tmp = ifeelgood[i];
                    i += 1;

                    if(i > 50000)
                        i = 0;
                    
                }

                //if(i > 65000)
                //    ison = 0;
            }else{
                i = 0;
            }

            //tmp = vol * ( softsquare[i] + (softsquare[i2]>>1) );
            //i += (uint16_t) (pluss);
            //i2 += (uint16_t) (pluss*1.5);
            //i += 200;

            ///////tmp = tmp >> 2;

        }


        //if(ctrl_pressed)
            output_buffer[ii] =  (int16_t) tmp;
        //else
        //    output_buffer[ii] = 0;


        //i++;
        //output_buffer[ii] = (int16_t) (vol * phatsnare[i]) >> 8;
        
        //output_buffer[ii] = (int16_t) ( saw[i]) >> 2;
        

        if(vol < 80)
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


void read_buttons(void *userData)
{

    while(1){

        uint32_t tmpfreq = get_smoothed_adc_value(0);

        if(tmpfreq > 80)
            freq = tmpfreq;


        freqoffset = get_smoothed_adc_value(2);
        
        int16_t freqoffsetq = freqoffset/100;

        float freqoffsetf = freqoffsetq / 12.0f;

        freq_q = (float) (freq + 100) / 1860.0f;

        pluss = 45 * pow(2.0f, freq_q + freqoffsetf);

        uint64_t tmpvol = 0;
        tmpvol = (get_smoothed_adc_value(1) - 770);
        
        tmpvol = (tmpvol * tmpvol) >> 11;

        if(tmpvol > 0)
            targetvol = tmpvol >> 1;
        
        vTaskDelay(1);
    }


}


#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
#define EXAMPLE_ADC_OUTPUT_TYPE             ADC_DIGI_OUTPUT_FORMAT_TYPE1
#define EXAMPLE_ADC_GET_CHANNEL(p_data)     ((p_data)->type1.channel)
#define EXAMPLE_ADC_GET_DATA(p_data)        ((p_data)->type1.data)
#else
#define EXAMPLE_ADC_OUTPUT_TYPE             ADC_DIGI_OUTPUT_FORMAT_TYPE2
#define EXAMPLE_ADC_GET_CHANNEL(p_data)     ((p_data)->type2.channel)
#define EXAMPLE_ADC_GET_DATA(p_data)        ((p_data)->type2.data)
#endif


static void audio_task(void *userData)
{
    
    while(1) {

        


        //vTaskDelay(1);
        write_sound();




        //vTaskDelay(pdMS_TO_TICKS(20)); // Delay ms
    }
}


void timer_callback(void *param)
{
    //targetvol += 100;

    
    if(targetvol > vol)
            vol+=1;
        else 
            vol-=1;
        
}



void app_main(void)
{

    adc_ctx.mutex = xSemaphoreCreateMutex();
    configASSERT(adc_ctx.mutex != NULL);

    // 2. Initialize ADC Unit
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_ctx.adc_handle));

    // 3. Configure Channels & Filters
    adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    for (int i = 0; i < NUM_CHANNELS; i++) {
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_ctx.adc_handle, channels[i], &config));
        moving_average_init(&adc_ctx.filters[i], filter_buffers[i], WINDOW_SIZE);
        adc_ctx.smoothed_vals[i] = 0;
    }

    // 4. Spawn the dedicated ADC Task
    xTaskCreate(
        adc_sampling_task,   // Task function
        "adc_sample_task",   // Name for debugging
        4096,                // Stack size in bytes (4KB is safe for ESP_LOGI)
        &adc_ctx,            // Pass context struct to task
        5,                   // Task priority (moderately high for steady timing)
        NULL                 // Optional task handle
    );


    const esp_timer_create_args_t my_timer_args = 
    {
        .callback = &timer_callback,
        .name = "Timer Interrupt"
    };

    esp_timer_handle_t timer_handler;
    esp_timer_create(&my_timer_args, &timer_handler);
    esp_timer_start_periodic(timer_handler, 100);  // One Second = 1000000 micro second


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
    
    xTaskCreate(audio_task, "audio", 4096, NULL, 2 | portPRIVILEGE_BIT, NULL);
    
    xTaskCreate(read_buttons, "read_buttons", 4096, NULL, ( 1 | portPRIVILEGE_BIT ), NULL);

    //xTaskCreate(vol_envelope, "volume", 4096, NULL, ( 1 | portPRIVILEGE_BIT ), NULL);
}
