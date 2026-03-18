#include "log.h"
#include "board.h"
#include "periph/gpio.h"
#include "ztimer.h"
#include "sema.h"
#include "thread.h"
#include "nec_protocol.h"

uint8_t recv_buffer[1024]= {0};
// ++++++++++++++++++++++++++ interrupt config ++++++++++++++++++++++++++ //
#define DEBOUNCE_DELAY_US 10                                // debounce delay in us
static bool signal                      = false;            // true: falling edge, false: rising edge
static ztimer_now_t interrupt_time_us   = 0;                // time of the last interrupt
gpio_t rec_pin                          = GPIO_PIN(0,4);    // Interrupt pin A0


// ++++++++++++++++++++++++++ threading config ++++++++++++++++++++++++++ //
static char receive_thread_stack[THREAD_STACKSIZE_MAIN];
nec_protocol_context_t nec_ctx;
static sema_t sema = SEMA_CREATE_LOCKED();



// ++++++++++++++++++++++++++ functions for infrared ++++++++++++++++++++++++++ //

/**
 * @brief   Interrupt callback function for the infrared device.
 *
 * This function is called when an interrupt occurs on the infrared device pin.
 * It reads the state of the pin and records the time of the interrupt. It then
 * posts to the semaphore to notify the receive thread of the interrupt.
 *
 * @param   arg Unused argument.
 */
void interrupt_callback(void* arg)
{
    (void)arg;
    signal = gpio_read(rec_pin);
    interrupt_time_us = ztimer_now(ZTIMER_USEC);
    sema_post(&sema);
}

/**
 * @brief   Receive thread function.
 *
 * This function is called by the receive thread and is responsible for
 * handling interrupts from the infrared device. It waits for interrupts
 * to occur, calculates the duration of the pulse and calls the
 * @ref nec_protocol_handle_event function to handle the event.
 *
 * @param   arg Unused argument.
 */
void *receive_thread(void* arg)
{   
    ztimer_now_t rising_time = 0;
    ztimer_now_t falling_time = 0;
    ztimer_now_t duration = 0;
    (void)arg;
    while (true)
    {
        sema_wait(&sema);
        if(!signal){
            falling_time = interrupt_time_us;
            duration = falling_time - rising_time;
        }else{
            rising_time = interrupt_time_us;
            duration = rising_time - falling_time;
        }
        nec_protocol_handle_event(signal, duration, &nec_ctx);
    }
}

/**
 * @brief   Initialize the infrared device.
 *
 * This function initializes the infrared device by setting up the pwm pin
 * and the interrupt. It also initializes the nec protocol and sets up a
 * receive thread to handle interrupts from the infrared device.
 *
 * @return  true if the initialization was successful, false otherwise.
 */
bool init_infrared(void)
{
    LOG_INFO("Initializing infrared device...\n");
    
    // setup interrupt
    if(gpio_init_int(rec_pin, GPIO_IN_PU, GPIO_BOTH, interrupt_callback, NULL) != 0){
        LOG_ERROR("Initializing interrupt failed!\n");
        return false;
    }
    LOG_INFO("Interrupt initialized!\n");
    
    // nec protocol init
    nec_protocol_init(&nec_ctx, recv_buffer, sizeof(recv_buffer), NULL);
    LOG_INFO("After init, nec_ctx.send_pulse: %p\n", (void*)nec_ctx.send_pulse);
    
    // setup receive thread
    thread_create(receive_thread_stack, sizeof(receive_thread_stack), THREAD_PRIORITY_MAIN - 1, 0, receive_thread, NULL,"Receive Thread");
    
    LOG_INFO("Infrared initialized!\n");
    return true;
}

int main(void)
{
    ztimer_sleep(ZTIMER_MSEC, 1000);
    LOG_INFO("\n\n\nStarting system...\n");
    
    if(!init_infrared()){
        LOG_ERROR("Initializing infrared device failed!\n");
        while (1){}
    }


    uint16_t count = 0;
    int byte = 0;
    while (true)
    {   
        if(tsrb_avail(&nec_ctx.recv_buffer) > 0)
        {
            LOG_INFO("Received message:");
            while ((byte =tsrb_get_one(&nec_ctx.recv_buffer)))
            {
                LOG_INFO("%c", (char)byte);
            }
            LOG_INFO("[%d]\n", count);
            count++;
        }
        ztimer_sleep(ZTIMER_MSEC,100);
    }
}