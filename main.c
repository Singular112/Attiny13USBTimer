/*
 * Attiny13USBTimer.c
 *
 * Created: 21.02.2020 11:31:49
 * Author : Singularity
 */ 

//#define F_CPU	(1200000UL) // 1.2 MHz
#define F_CPU	(9600000UL) // 9.6 MHz

//#define MODE_SETUP_CURRENT_TIMESTAMP
//#define MODE_ONLY_UART
//#define MODE_DEBUG_TIMESTAMPS

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>

#include "uart.h"
#include <string.h>

#include "ds1302_avr.h"

uint8_t g_current_timestamp[DATETIME_DATA_SIZE]	= {};
uint8_t g_offline_datetime[DATETIME_DATA_SIZE]	= {};
uint8_t g_online_datetime[DATETIME_DATA_SIZE]	= {};

const uint8_t g_defalt_datetime[DATETIME_DATA_SIZE] =
	// sec		min		hour	day		mon		dow(1-7)	year	wp (in BCD!)
	{ 0x00, 0x11, 0x21, 0x26, 0x04, 0x04,    0x18,  0x00 };

uint8_t g_timer_enabled = 1;

// Convert Decimal to Binary Coded Decimal (BCD)
uint8_t dec2bcd(uint8_t num)
{
	return ((num/10 * 16) + (num % 10));
}


// Convert Binary Coded Decimal (BCD) to Decimal
uint8_t bcd2dec(uint8_t num)
{
	return ((num/16 * 10) + (num % 16));
}


void initialize_hardware()
{
	// Crystal Oscillator division factor: 1
	CLKPR = (1 << CLKPCE);
	CLKPR = (0 << CLKPCE) | (0 << CLKPS3) | (0 << CLKPS2) | (0 << CLKPS1) | (0 << CLKPS0);

	// Input/Output Ports initialization
	// Port B initialization
	// Function
	DDRB = (0 << DDB5)
		| (0 << DDB4)
		| (1 << DDB3)	//	 out
		| (0 << DDB2)
		| (0 << DDB1)
		| (0 << DDB0);

	// State
	PORTB = (0 << PORTB5)
		| (1 << PORTB4)	//	pullup
		| (0 << PORTB3)
		| (0 << PORTB2)
		| (0 << PORTB1)
		| (0 << PORTB0);

#if 0
	// Watchdog Timer initialization
	// Watchdog Timer Prescaler: OSC/1024k
	// Watchdog timeout action: Reset
	__asm volatile("wdr");
	WDTCR |= (1 << WDCE) | (1 << WDE);
	WDTCR = (0 << WDTIF)
		| (0 << WDTIE)
		| (1 << WDP3)
		| (0 << WDCE)
		| (1 << WDE)
		| (0 << WDP2)
		| (0 << WDP1)
		| (1 << WDP0);
#endif

	// Clock
	ds1302_init();
}


uint8_t check_time(const uint8_t* dt_ptr)
{
	// check overflow
	if (bcd2dec(dt_ptr[0]) <= 59 &&
		bcd2dec(dt_ptr[1]) <= 59 &&
		bcd2dec(dt_ptr[2]) <= 23)
	{
		return 1;
	}

	return 0;
}


void on_online_time_event()
{
	PORTB |= (1 << PORTB3);
}


void on_offline_time_event()
{
	PORTB &= ~(1 << PORTB3);
}


void read_current_time()
{
	ds1302_select();

	ds1302_transmit_byte(DS1302_CLOCK_BURST_READ);
	for(uint8_t i = 0; i < sizeof(g_current_timestamp); i++)
	{
		g_current_timestamp[i] = ds1302_receive_byte();
	}

	ds1302_deselect();	
}


static char time_buf[4];
void send_time_to_uart(register const uint8_t val)
{
	itoa(bcd2dec(g_current_timestamp[val] & 0x7F), time_buf, 10);
	uart_puts(time_buf);
	uart_putc(':');
}


void send_int_to_uart(register const uint16_t val)
{
	itoa(bcd2dec(g_current_timestamp[val]), time_buf, 10);
	uart_puts(time_buf);
}


void send_to_uart_current_time()
{
	read_current_time();
	send_time_to_uart(HOURS_BYTE);
	send_time_to_uart(MINUTES_BYTE);
#if 0
	for (uint8_t i = 2; i < DATETIME_DATA_SIZE; ++i)
		send_time_to_uart(SECONDS_BYTE);
	uart_putc('\n');
#endif
}


void blink_portb3()
{
	PORTB |= (1 << PORTB3);
	_delay_ms(500);
	PORTB &= ~(1 << PORTB3);
	_delay_ms(500);
}


static char g_byte_sequence[4];
void switch_to_uart()
{
	// blink 3 times
	if (1)
	{
		for (uint8_t i = 0; i < 3; ++i)
			blink_portb3();
		send_to_uart_current_time();
	}

	// configure ports
	PORTB = (0 << PORTB3) | (0 << PORTB4);

	while (1)
	{
		uint8_t read_cnt = 0;
		// read time settings in bcd-format
		for (uint8_t i = 0; i < sizeof(g_byte_sequence); ++i)
		{
			g_byte_sequence[i] = uart_getc();
			
			uint8_t ch = g_byte_sequence[i];
			if (ch >= 'a' && ch <= 132)	// data in bcd + 'a'
			{
				g_byte_sequence[i] -= 'a';
				++read_cnt;
			}
		}

		if (read_cnt == sizeof(g_byte_sequence))	// only if all byte sequence correct received
		{
			uart_puts("ok");	// ok

			g_online_datetime[HOURS_BYTE] = g_byte_sequence[0];
			g_online_datetime[MINUTES_BYTE] = g_byte_sequence[1];

			g_offline_datetime[HOURS_BYTE] = g_byte_sequence[2];
			g_offline_datetime[MINUTES_BYTE] = g_byte_sequence[3];

			eeprom_write_block(g_online_datetime,
				(void*)0x00,
				sizeof(g_online_datetime));

			eeprom_write_block(g_offline_datetime,
				(void*)0x00 + sizeof(g_offline_datetime),
				sizeof(g_offline_datetime));
			break;
		}
		else
		{
			if (g_byte_sequence[0] == 'e')
			{
				uart_puts("ex");	// exit
				break;
			}
			uart_puts("fal");		// fal
		}
	}

	// configure ports
	PORTB |= (1 << PORTB4);		//	PORTB4 pullup;
}


uint16_t timestamp_to_minutes(const uint8_t* ts)
{
	return (uint16_t)0 | (((uint16_t)bcd2dec(ts[HOURS_BYTE])) << 8) | (((uint16_t)bcd2dec(ts[MINUTES_BYTE])));
}


enum e_eeprom_shift_ptr
{
	esp_online_datetime		= 0,
	esp_offline_datetime	= DATETIME_DATA_SIZE + 1,
	esp_enabled				= esp_offline_datetime + DATETIME_DATA_SIZE + 1
};


int main(void)
{
	//uart_puts("begin\n");

	initialize_hardware();
	
//#define MODE_SETUP_CURRENT_TIMESTAMP
#ifdef MODE_SETUP_CURRENT_TIMESTAMP

	uint8_t new_datetime[DATETIME_DATA_SIZE] =
		// sec			min				hour			day		mon		dow(1-7)	year	wp (in BCD!)
		{ dec2bcd(0),	dec2bcd(55),	dec2bcd(18),	0x26,	0x04,	0x04,		0x18,	0x00 };

	uint8_t online_time[DATETIME_DATA_SIZE] =
		// sec			min				hour			day		mon		dow(1-7)	year	wp (in BCD!)
		{ dec2bcd(0),	dec2bcd(30),	dec2bcd(7),		0x26,	0x04,	0x04,		0x18,	0x00 };

	uint8_t offline_time[DATETIME_DATA_SIZE] =
		// sec			min				hour			day		mon		dow(1-7)	year	wp (in BCD!)
		{ dec2bcd(0),	dec2bcd(0),		dec2bcd(23),	0x26,	0x04,	0x04,		0x18,	0x00 };

	ds1302_init();
	//switch_to_uart();

	ds1302_write_enable(1);
	ds1302_halt(1);
	//uart_putc(ds1302_is_halted() + '0');

	ds1302_set_time(new_datetime);

#if 0
	read_current_time();
	int result = memcmp(new_datetime, g_current_timestamp, sizeof(new_datetime));
	{
		uint8_t ntimes = (result == 0 ? 5 : 2);
		for (int i = 0; i < ntimes; ++i)
		{
			blink_portb3();
		}
	}
#endif

	ds1302_halt(0);
	ds1302_write_enable(0);

	eeprom_write_block(online_time,
		(void*)esp_online_datetime,
		sizeof(online_time));

	eeprom_write_block(offline_time,
		(void*)esp_offline_datetime,
		sizeof(offline_time));
#if 0
	eeprom_read_block(g_online_datetime,
			(const void*)esp_online_datetime,
			sizeof(g_online_datetime));

	eeprom_read_block(g_offline_datetime,
		(const void*)esp_offline_datetime,
		sizeof(g_offline_datetime));
#endif

#if 0
	int result = memcmp(online_time, g_online_datetime, sizeof(online_time)) == 0
		&& memcmp(offline_time, g_offline_datetime, sizeof(offline_time)) == 0;
	{
		uint8_t ntimes = (result == 1 ? 5 : 2);
		for (int i = 0; i < ntimes; ++i)
		{
			blink_portb3();
		}
	}
#endif

	//uart_putc(ds1302_is_halted() + '0');

#if 0
	uint8_t ntimes = (ds1302_is_halted() ? 5 : 2);
	for (int i = 0; i < ntimes; ++i)
	{
		blink_portb3();
	}
#endif
	//if (ds1302_is_halted())
	//{
		////uart_puts("halted\n");
	//}
	//else
	//{
		////uart_puts("not halted\n");
	//}

	return 0;

#endif

#ifdef MODE_ONLY_UART
	switch_to_uart();
	return;
#endif

	// read settings
	{
		eeprom_read_block(g_online_datetime,
			(const void*)esp_online_datetime,
			sizeof(g_online_datetime));

		eeprom_read_block(g_offline_datetime,
			(const void*)esp_offline_datetime,
			sizeof(g_offline_datetime));

		g_timer_enabled = eeprom_read_byte((const uint8_t*)esp_enabled);
		if (g_timer_enabled > 1)
		{
			g_timer_enabled = 1;
			eeprom_write_byte((uint8_t*)esp_enabled, g_timer_enabled);
		}

#if 0
		if (check_time(g_online_datetime) == 0 || check_time(g_offline_datetime) == 0)
		{
			for (int i = 0; i < 5; ++i)
			{
				blink_portb3();
			}

			memcpy(g_online_datetime, g_defalt_datetime, sizeof(g_defalt_datetime));
			memcpy(g_offline_datetime, g_defalt_datetime, sizeof(g_defalt_datetime));

			eeprom_write_block(g_online_datetime,
				(void*)esp_online_datetime,
				sizeof(g_online_datetime));

			eeprom_write_block(g_offline_datetime,
				(void*)esp_offline_datetime,
				sizeof(g_offline_datetime));
		}
#endif
	}

	while (1)
	{
		_delay_ms(100);

#if 0
		uint8_t count = 0;
		// while button pressed
		while ((PINB & (1 << PORTB4)) == 0)
		{
			on_offline_time_event();
			_delay_ms(1000);
			if (++count > 3)
			{
				switch_to_uart();
				break;
			}
		}
#endif

#if 1
		uint8_t count = 0;
		// while button pressed
		while ((PINB & (1 << PORTB4)) == 0)
		{
			on_offline_time_event();
			_delay_ms(1000);
			if (++count > 3)
			{
				//_delay_ms(3000);

				g_timer_enabled = 1 - g_timer_enabled;
				eeprom_write_byte((uint8_t*)esp_enabled, g_timer_enabled);

				uint8_t ntimes = (g_timer_enabled == 1 ? 5 : 2);
				for (int i = 0; i < ntimes; ++i)
				{
					blink_portb3();
				}
				break;
			}
		}
#endif

		if (!g_timer_enabled)
		{
			on_online_time_event();
			continue;
		}

		read_current_time();

#if 1	// optimized
		/*
		Program Memory Usage 	:	628 bytes   61,3 % Full
		Data Memory Usage 		:	40 bytes   62,5 % Full
		*/
		const uint16_t current_timestamp_num_minutes	= timestamp_to_minutes(g_current_timestamp);
		uint16_t leftbound_timestamp_num_minutes		= timestamp_to_minutes(g_online_datetime);
		uint16_t rightbound_timestamp_num_minutes		= timestamp_to_minutes(g_offline_datetime);
#else
		/*
		Program Memory Usage 	:	678 bytes   66,2 % Full
		Data Memory Usage 		:	40 bytes   62,5 % Full
		*/
		const uint16_t current_timestamp_num_minutes	= g_current_timestamp[HOURS_BYTE] * 60 + g_current_timestamp[MINUTES_BYTE];
		uint16_t leftbound_timestamp_num_minutes		= g_online_datetime[HOURS_BYTE] * 60 + g_online_datetime[MINUTES_BYTE];
		uint16_t rightbound_timestamp_num_minutes		= g_offline_datetime[HOURS_BYTE] * 60 + g_offline_datetime[MINUTES_BYTE];
#endif

#ifdef MODE_DEBUG_TIMESTAMPS
	_delay_ms(1000);
	send_to_uart_current_time();
	_delay_ms(1000);
	uart_putc('\n');
	for (uint8_t i = 0; i < 8; ++i)
	{
			itoa(g_current_timestamp[i], time_buf, 10);
			uart_puts(time_buf);
			uart_putc('|');
	}
	uart_putc('\n');
	for (uint8_t i = 0; i < 8; ++i)
	{
		itoa(g_online_datetime[i], time_buf, 10);
		uart_puts(time_buf);
		uart_putc('|');
	}
	uart_putc('\n');
	for (uint8_t i = 0; i < 8; ++i)
	{
		itoa(g_offline_datetime[i], time_buf, 10);
		uart_puts(time_buf);
		uart_putc('|');
	}

		uart_putc('\n');
		itoa(current_timestamp_num_minutes, time_buf, 10);
		uart_puts(time_buf);
		uart_putc(':');
		itoa(leftbound_timestamp_num_minutes, time_buf, 10);
		uart_puts(time_buf);
		uart_putc(':');
		itoa(rightbound_timestamp_num_minutes, time_buf, 10);
		uart_puts(time_buf);
		uart_putc('\n');
		return;
#endif

#if 1	// optimized
		/*
		Program Memory Usage 	:	626 bytes   61,1 % Full
		Data Memory Usage 		:	40 bytes   62,5 % Full
		*/

		if (leftbound_timestamp_num_minutes < rightbound_timestamp_num_minutes)	// no rotated
		{
			if (current_timestamp_num_minutes >= leftbound_timestamp_num_minutes
				&&
				current_timestamp_num_minutes <= rightbound_timestamp_num_minutes)
			{
				// online time
				on_online_time_event();
			}
			else
			{
				// offline time
				on_offline_time_event();
			}
		}
		else
		{
			if (current_timestamp_num_minutes >= rightbound_timestamp_num_minutes
				&&
				current_timestamp_num_minutes <= leftbound_timestamp_num_minutes)
			{
				// offline time
				on_offline_time_event();
			}
			else
			{
				// online time
				on_online_time_event();
			}
		}
#else
		/*
		Program Memory Usage 	:	636 bytes   62,1 % Full
		Data Memory Usage 		:	40 bytes   62,5 % Full
		*/

		const uint8_t is_end_begin_rotated = leftbound_timestamp_num_minutes > rightbound_timestamp_num_minutes;
		if (is_end_begin_rotated)
		{
			const uint16_t swap_buf = leftbound_timestamp_num_minutes;
			leftbound_timestamp_num_minutes = rightbound_timestamp_num_minutes;
			rightbound_timestamp_num_minutes = swap_buf;
		}

		if (current_timestamp_num_minutes >= leftbound_timestamp_num_minutes
			&&
			current_timestamp_num_minutes <= rightbound_timestamp_num_minutes)
		{
			if (is_end_begin_rotated)
			{
				// offline time
				on_offline_time_event();
			}
			else
			{
				// online time
				on_online_time_event();
			}
		}
		else
		{
			if (is_end_begin_rotated)
			{
				// online time
				on_online_time_event();
			}
			else
			{
				// offline time
				on_offline_time_event();
			}
		}
#endif
	}
}
