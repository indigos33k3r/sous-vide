/*
 * This file is part of GreatFET
 */

#include "usb_api_sous_vide.h"
#include "usb_api_DS18B20.h"
#include <libopencm3/lpc43xx/rtc.h>

#include <gpio.h>
#include <gpio_lpc.h>

#include "usb.h"
#include "usb_queue.h"
#include "usb_endpoint.h"

#include "pins.h"

#include <libopencm3/lpc43xx/scu.h>

// GPIO 2_2 is J2_P8 on greatfet
static struct gpio_t heaters = GPIO(2, 2);
volatile bool sous_vide_mode_enabled = true;

#define COOK_TIME 3600
#define TARGET_TEMPERATURE 82
#define MIN_TEMPERATURE 79
#define MAX_TEMPERATURE 85
#define NOW_SEC ((RTC_HRS * 3600) + (RTC_MIN * 60) + RTC_SEC)
#define DELAY_TIME 40000000

static uint32_t start_time = 0;		
static uint32_t time_elapsed = 0;	
static int16_t current_temperature = 0;
static bool timer_started = false;
static bool cook_completed = false;

void init_cook(void) {
	scu_pinmux(SCU_PINMUX_GPIO2_2, SCU_GPIO_FAST | SCU_CONF_FUNCTION0);
	gpio_output(&heaters); 
	turn_leds_off();

	led_on(LED1);
	led_on(LED2);
	// delay(DELAY_TIME);
	
	// TODO: need to wait for button press to start cook process
	// TODO: need to reset cook_completed to false on button press
	if(!cook_completed) {
		heating_up();
	}
	else {
		done();
	}
}

void heating_up() {
	turn_leds_off();
	turn_on_heater();
	led_on(LED1);

	while(current_temperature < TARGET_TEMPERATURE && time_elapsed < COOK_TIME) {
		// heating up
		current_temperature = read_temperature();
		current_temperature >>= 4;

		if(timer_started) {
			turn_leds_off();
			led_on(LED1);
			led_on(LED3);
			// delay(DELAY_TIME);
			time_elapsed = get_time_elapsed();	
		}
		delay(DELAY_TIME);
	}
	if(!timer_started) {
		turn_leds_off();
		start_time = get_start_time();
		timer_started = true;
		turn_off_heater();
		cooking();
	}
	else if(timer_started && time_elapsed < COOK_TIME) {
		turn_leds_off();
		time_elapsed = get_time_elapsed();
		turn_off_heater();
		cooking();
	}
	else if(timer_started && time_elapsed >= COOK_TIME) {
		turn_leds_off();
		led_on(LED2);
		led_on(LED3);
		turn_off_heater();
		// delay(DELAY_TIME);
		done();
	}
}

void cooking() {
	turn_leds_off();
	led_on(LED2);

	while(current_temperature > MIN_TEMPERATURE && time_elapsed < COOK_TIME) {
		time_elapsed = get_time_elapsed();
		current_temperature = read_temperature();
		current_temperature >>= 4;
		delay(DELAY_TIME);
	}
	if(time_elapsed >= COOK_TIME) {
		turn_leds_off();
		led_on(LED2);
		led_on(LED4);
		// delay(DELAY_TIME);
		done();
	}
	else if(current_temperature <= MIN_TEMPERATURE) {
		heating_up();
	}
}

void done(void) {
	cook_completed = true;
	turn_leds_on();
}

void turn_on_heater(void) {
	gpio_write(&heaters, 1);
}

void turn_off_heater(void) {
	gpio_write(&heaters, 0);
}

uint32_t get_start_time(void) {
	return NOW_SEC;
}

uint32_t get_time_elapsed() {
	uint32_t current_time = NOW_SEC;
	uint32_t time_elapsed = current_time - start_time;

	return time_elapsed;
}

void turn_leds_on(void) {
	led_on(LED1);
	led_on(LED2);
	led_on(LED3);
	led_on(LED4);
}

void turn_leds_off(void) {
	led_off(LED1);
	led_off(LED2);
	led_off(LED3);
	led_off(LED4);
}

/* Start the creme brulee cook process
   This is called from the main loop. */
void sous_vide_mode(void) {
	init_cook();
}

usb_request_status_t usb_vendor_request_sous_vide_start(
	usb_endpoint_t* const endpoint, const usb_transfer_stage_t stage)
{
	if (stage == USB_TRANSFER_STAGE_SETUP) {
		sous_vide_mode_enabled = true;
    //led_off(HEARTBEAT_LED);
		usb_transfer_schedule_ack(endpoint->in);
	}
	return USB_REQUEST_STATUS_OK;
}