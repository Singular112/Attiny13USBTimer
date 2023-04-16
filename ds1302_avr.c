/*
 * ds1302_avr.c
 *
 * Created: 23.02.2020 17:18:35
 *  Author: Singularity
 */ 

#include "ds1302_avr.h"

void ds1302_init(void)
{
	// CE - output, set low
	DS1302_CE_DDR |= (1 << DS1302_CE);
	DS1302_CE_PORT &= ~(1 << DS1302_CE);

	// CLK - output, set low
	DS1302_CLK_DDR |= (1 << DS1302_CLK);
	DS1302_CLK_PORT &= ~(1 << DS1302_CLK);

	// DIO - output, set low (for now)
	DS1302_DIO_DDR |= (1 << DS1302_DIO);
	DS1302_DIO_PORT &= ~(1 << DS1302_DIO);
}


void ds1302_select(void)
{
	// set CE high
	DS1302_CE_PORT |= (1 << DS1302_CE);
}


void ds1302_deselect(void)
{
	// set CE low
	DS1302_CE_PORT &= ~(1 << DS1302_CE);
}


void ds1302_transmit_byte(uint8_t byte)
{
	// DIO - output, set low
	DS1302_DIO_DDR |= (1 << DS1302_DIO);
	DS1302_DIO_PORT &= ~(1 << DS1302_DIO);

	// transmit byte, lsb-first
	for (uint8_t i = 0; i < 8; i++)
	{
		if ((byte >> i) & 0x01)
		{
			// set high
			DS1302_DIO_PORT |= (1 << DS1302_DIO);
		}
		else
		{
			// set low
			DS1302_DIO_PORT &= ~(1 << DS1302_DIO);
		}

		// send CLK signal
		DS1302_CLK_PORT |= (1 << DS1302_CLK);
		_delay_us(DS1302_DELAY_USEC);
		DS1302_CLK_PORT &= ~(1 << DS1302_CLK);
	}
}


uint8_t ds1302_receive_byte(void)
{
	// DIO - input
	DS1302_DIO_DDR &= ~(1 << DS1302_DIO);

	// NB: receive is always done after transmit, thus
	// falling edge of CLK signal was already sent
	// see "Figure 4. Data Transfer Summary" for more details

	// receive byte, lsb-first
	uint8_t byte = 0;
	for (uint8_t i = 0; i < 8; i++)
	{
		if (DS1302_DIO_PIN & (1 << DS1302_DIO))
		{
			byte |= (1 << i);
		}

		// send CLK signal
		DS1302_CLK_PORT |= (1 << DS1302_CLK);
		_delay_us(DS1302_DELAY_USEC);
		DS1302_CLK_PORT &= ~(1 << DS1302_CLK);
	}

	return byte;
}


void ds1302_set_time(const uint8_t* dt_ptr)
{
	ds1302_select();

	ds1302_transmit_byte(DS1302_CLOCK_BURST);

	for (uint8_t i = 0; i < DATETIME_DATA_SIZE; i++)
	{
		ds1302_transmit_byte(dt_ptr[i]);
	}

	ds1302_deselect();
}


uint8_t ds1302_read(uint8_t address)
{
	bitSet(address, 0);
	ds1302_select();
	ds1302_transmit_byte(address);
	uint8_t value = ds1302_receive_byte();
	ds1302_deselect();
	return value;
}


uint8_t ds1302_is_halted()
{
	return bitRead(ds1302_read(DS1302_SECONDS), 7);
}


void ds1302_halt(uint8_t value)
{
	uint8_t seconds = ds1302_read(DS1302_SECONDS);
	bitClear(seconds, 7);
	bitWrite(seconds, 7, value);
	ds1302_select();
	ds1302_transmit_byte(DS1302_SECONDS);
	ds1302_transmit_byte(seconds);
	ds1302_deselect();
}


void ds1302_write_enable(uint8_t value)
{
	uint8_t wp = ds1302_read(DS1302_ENABLE);
	bitClear(wp, 7);
	bitWrite(wp, 7, !value);
	ds1302_select();
	ds1302_transmit_byte(DS1302_ENABLE);
	ds1302_transmit_byte(wp);
	ds1302_deselect();
}


uint8_t ds1302_is_write_enable()
{
	return !bitRead(ds1302_read(DS1302_ENABLE), 7);
}
