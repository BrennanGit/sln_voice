// Copyright (c) 2020-2022 XMOS LIMITED. This Software is subject to the terms of the
// XMOS Public License: Version 1

#include <platform.h>
#include <xs1.h>
#include <xcore/channel.h>

/* FreeRTOS headers */
#include "FreeRTOS.h"
#include "task.h"
#include "stream_buffer.h"
#include "queue.h"

/* Library headers */
#include "rtos_printf.h"
#include "src.h"

/* App headers */
#include "app_conf.h"
#include "platform/platform_init.h"
#include "platform/driver_instances.h"
#include "usb_support.h"
#include "usb_audio.h"
#include "audio_pipeline.h"
#include "ww_model_runner/ww_model_runner.h"
#include "fs_support.h"

#include "gpio_test/gpio_test.h"
#include "pipeline.h"

volatile int mic_from_usb = appconfMIC_SRC_DEFAULT;
volatile int aec_ref_source = appconfAEC_REF_DEFAULT;

#if appconfI2S_ENABLED && (appconfI2S_MODE == appconfI2S_MODE_SLAVE)
void i2s_slave_intertile(void *args) {
    (void) args;
    int32_t tmp[appconfAUDIO_PIPELINE_FRAME_ADVANCE][appconfAUDIO_PIPELINE_CHANNELS];

    while(1) {
        memset(tmp, 0x00, sizeof(tmp));

        size_t bytes_received = 0;
        bytes_received = rtos_intertile_rx_len(
                intertile_ctx,
                appconfI2S_OUTPUT_SLAVE_PORT,
                portMAX_DELAY);

        xassert(bytes_received == sizeof(tmp));

        rtos_intertile_rx_data(
                intertile_ctx,
                tmp,
                bytes_received);

        rtos_i2s_tx(i2s_ctx,
                    (int32_t*) tmp,
                    appconfAUDIO_PIPELINE_FRAME_ADVANCE,
                    portMAX_DELAY);
    }
}
#endif

RTOS_I2S_APP_SEND_FILTER_CALLBACK_ATTR
size_t i2s_send_upsample_cb(rtos_i2s_t *ctx, void *app_data, int32_t *i2s_frame, size_t i2s_frame_size, int32_t *send_buf, size_t samples_available)
{
    static int i;
    static int32_t src_data[2][SRC_FF3V_FIR_TAPS_PER_PHASE] __attribute__((aligned(8)));

    xassert(i2s_frame_size == 2);

    switch (i) {
    case 0:
        i = 1;
        if (samples_available >= 2) {
            i2s_frame[0] = src_us3_voice_input_sample(src_data[0], src_ff3v_fir_coefs[2], send_buf[0]);
            i2s_frame[1] = src_us3_voice_input_sample(src_data[1], src_ff3v_fir_coefs[2], send_buf[1]);
            return 2;
        } else {
            i2s_frame[0] = src_us3_voice_input_sample(src_data[0], src_ff3v_fir_coefs[2], 0);
            i2s_frame[1] = src_us3_voice_input_sample(src_data[1], src_ff3v_fir_coefs[2], 0);
            return 0;
        }
    case 1:
        i = 2;
        i2s_frame[0] = src_us3_voice_get_next_sample(src_data[0], src_ff3v_fir_coefs[1]);
        i2s_frame[1] = src_us3_voice_get_next_sample(src_data[1], src_ff3v_fir_coefs[1]);
        return 0;
    case 2:
        i = 0;
        i2s_frame[0] = src_us3_voice_get_next_sample(src_data[0], src_ff3v_fir_coefs[0]);
        i2s_frame[1] = src_us3_voice_get_next_sample(src_data[1], src_ff3v_fir_coefs[0]);
        return 0;
    default:
        xassert(0);
        return 0;
    }
}

RTOS_I2S_APP_RECEIVE_FILTER_CALLBACK_ATTR
size_t i2s_send_downsample_cb(rtos_i2s_t *ctx, void *app_data, int32_t *i2s_frame, size_t i2s_frame_size, int32_t *receive_buf, size_t sample_spaces_free)
{
    static int i;
    static int64_t sum[2];
    static int32_t src_data[2][SRC_FF3V_FIR_NUM_PHASES][SRC_FF3V_FIR_TAPS_PER_PHASE] __attribute__((aligned (8)));

    xassert(i2s_frame_size == 2);

    switch (i) {
    case 0:
        i = 1;
        sum[0] = src_ds3_voice_add_sample(0, src_data[0][0], src_ff3v_fir_coefs[0], i2s_frame[0]);
        sum[1] = src_ds3_voice_add_sample(0, src_data[1][0], src_ff3v_fir_coefs[0], i2s_frame[1]);
        return 0;
    case 1:
        i = 2;
        sum[0] = src_ds3_voice_add_sample(sum[0], src_data[0][1], src_ff3v_fir_coefs[1], i2s_frame[0]);
        sum[1] = src_ds3_voice_add_sample(sum[1], src_data[1][1], src_ff3v_fir_coefs[1], i2s_frame[1]);
        return 0;
    case 2:
        i = 0;
        if (sample_spaces_free >= 2) {
            receive_buf[0] = src_ds3_voice_add_final_sample(sum[0], src_data[0][2], src_ff3v_fir_coefs[2], i2s_frame[0]);
            receive_buf[1] = src_ds3_voice_add_final_sample(sum[1], src_data[1][2], src_ff3v_fir_coefs[2], i2s_frame[1]);
            return 2;
        } else {
            (void) src_ds3_voice_add_final_sample(sum[0], src_data[0][2], src_ff3v_fir_coefs[2], i2s_frame[0]);
            (void) src_ds3_voice_add_final_sample(sum[1], src_data[1][2], src_ff3v_fir_coefs[2], i2s_frame[1]);
            return 0;
        }
    default:
        xassert(0);
        return 0;
    }
}

void i2s_rate_conversion_enable(void)
{
#if !appconfI2S_TDM_ENABLED
    rtos_i2s_send_filter_cb_set(i2s_ctx, i2s_send_upsample_cb, NULL);
#endif
    rtos_i2s_receive_filter_cb_set(i2s_ctx, i2s_send_downsample_cb, NULL);
}

void vApplicationMallocFailedHook(void)
{
    rtos_printf("Malloc Failed on tile %d!\n", THIS_XCORE_TILE);
    xassert(0);
    for(;;);
}

static void mem_analysis(void)
{
	for (;;) {
		rtos_printf("Tile[%d]:\n\tMinimum heap free: %d\n\tCurrent heap free: %d\n", THIS_XCORE_TILE, xPortGetMinimumEverFreeHeapSize(), xPortGetFreeHeapSize());
		vTaskDelay(pdMS_TO_TICKS(5000));
	}
}

void startup_task(void *arg)
{
    rtos_printf("Startup task running from tile %d on core %d\n", THIS_XCORE_TILE, portGET_CORE_ID());

    platform_start();

#if ON_TILE(1) && appconfI2S_ENABLED && (appconfI2S_MODE == appconfI2S_MODE_SLAVE)
    xTaskCreate((TaskFunction_t) i2s_slave_intertile,
                "i2s_slave_intertile",
                RTOS_THREAD_STACK_SIZE(i2s_slave_intertile),
                NULL,
                appconfAUDIO_PIPELINE_TASK_PRIORITY,
                NULL);
#endif

#if ON_TILE(1)
    gpio_test(gpio_ctx_t0);
#endif

    //audio_pipeline_init(NULL, NULL);
#if ON_TILE(1)
    pipeline_init();
#endif

#if ON_TILE(FS_TILE_NO)
    rtos_fatfs_init(qspi_flash_ctx);
    rtos_dfu_image_print_debug(dfu_image_ctx);
#endif

#if appconfWW_ENABLED && ON_TILE(WW_TILE_NO)
    ww_task_create(appconfWW_TASK_PRIORITY);
#endif

    mem_analysis();
    /*
     * TODO: Watchdog?
     */
}

void vApplicationMinimalIdleHook(void)
{
    rtos_printf("idle hook on tile %d core %d\n", THIS_XCORE_TILE, rtos_core_id_get());
    asm volatile("waiteu");
}

static void tile_common_init(chanend_t c)
{
    platform_init(c);
    chanend_free(c);

#if appconfUSB_ENABLED && ON_TILE(USB_TILE_NO)
    usb_audio_init(intertile_usb_audio_ctx, appconfUSB_AUDIO_TASK_PRIORITY);
#endif

    xTaskCreate((TaskFunction_t) startup_task,
                "startup_task",
                RTOS_THREAD_STACK_SIZE(startup_task),
                NULL,
                appconfSTARTUP_TASK_PRIORITY,
                NULL);

    rtos_printf("start scheduler on tile %d\n", THIS_XCORE_TILE);
    vTaskStartScheduler();
}

#if ON_TILE(0)
void main_tile0(chanend_t c0, chanend_t c1, chanend_t c2, chanend_t c3)
{
    (void) c0;
    (void) c2;
    (void) c3;

    tile_common_init(c1);
}
#endif

#if ON_TILE(1)
void main_tile1(chanend_t c0, chanend_t c1, chanend_t c2, chanend_t c3)
{
    (void) c1;
    (void) c2;
    (void) c3;

    tile_common_init(c0);
}
#endif
