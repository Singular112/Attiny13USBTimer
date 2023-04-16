/*
 * ds1302_avr.h
 *
 * Created: 23.02.2020 17:19:31
 *  Author: Singularity
 */ 


#ifndef DS1302_AVR_H_
#define DS1302_AVR_H_

#define F_CPU	(9600000UL) // 9.6 MHz
#include <util/delay.h>
#include <avr/io.h>
#include <stdint.h>

// Register names.
// Since the highest bit is always '1',
// the registers start at 0x80
// If the register is read, the lowest bit should be '1'.
#define DS1302_SECONDS           0x80
#define DS1302_MINUTES           0x82
#define DS1302_HOURS             0x84
#define DS1302_DATE              0x86
#define DS1302_MONTH             0x88
#define DS1302_DAY               0x8A
#define DS1302_YEAR              0x8C
#define DS1302_ENABLE            0x8E
#define DS1302_TRICKLE           0x90
#define DS1302_CLOCK_BURST       0xBE
#define DS1302_CLOCK_BURST_WRITE 0xBE
#define DS1302_CLOCK_BURST_READ  0xBF
#define DS1302_RAM_START         0xC0
#define DS1302_RAM_END           0xFC
#define DS1302_RAM_BURST         0xFE
#define DS1302_RAM_BURST_WRITE   0xFE
#define DS1302_RAM_BURST_READ    0xFF

#define bitWrite(number, pos, value)	(number |= ((uint8_t)value << pos))
#define bitClear(number, pos)			(number &= ~((uint8_t)1 << pos))
#define bitSet(number, pos)				(number |= (uint8_t)1 << pos)
#define bitRead(number, pos)			((number >> pos) & 0x01)

#define DS1302_CLK			PORTB0
#define DS1302_CLK_DDR		DDRB
#define DS1302_CLK_PORT		PORTB

#define DS1302_DIO			PORTB1
#define DS1302_DIO_DDR		DDRB
#define DS1302_DIO_PORT		PORTB
#define DS1302_DIO_PIN		PINB

#define DS1302_CE			PORTB2
#define DS1302_CE_DDR		DDRB
#define DS1302_CE_PORT		PORTB

#define DS1302_DELAY_USEC	3

#define SECONDS_BYTE		0
#define MINUTES_BYTE		1
#define HOURS_BYTE			2

#define DATETIME_DATA_SIZE	8


void ds1302_init(void);

void ds1302_select(void);

void ds1302_deselect(void);

void ds1302_transmit_byte(uint8_t byte);

uint8_t ds1302_receive_byte(void);

void ds1302_set_time(const uint8_t* dt_ptr);

uint8_t ds1302_read(uint8_t address);

uint8_t ds1302_is_halted();

void ds1302_halt(uint8_t value);

void ds1302_write_enable(uint8_t value);

uint8_t ds1302_is_write_enable();


#endif /* DS1302_AVR_H_ */
