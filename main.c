/**
 * Digital LED termometer on ATTINY13, LED Display TM1637 and LM35
 * Measurement range -50 .. +100 Celsius degrees 
 * LM35 is pulled via 1N4007 on GND pin + pulldown 18KOhm to GND on Vout
 * for negative measurements 
*/

#include <stdint.h>
#include <avr/io.h>
#include <avr/pgmspace.h>


// Vout from LM35
#define	LM35_DATA_PIN	PB4
// GND from LM35, there is 1N4007 anode connected to it
#define LM35_GND_PIN    PB3

#define	TM1637_DIO_HIGH()		(PORTB |= _BV(TM1637_DIO_PIN))
#define	TM1637_DIO_LOW()		(PORTB &= ~_BV(TM1637_DIO_PIN))
#define	TM1637_DIO_OUTPUT()		(DDRB |= _BV(TM1637_DIO_PIN))
#define	TM1637_DIO_INPUT()		(DDRB &= ~_BV(TM1637_DIO_PIN))
#define	TM1637_DIO_READ() 		(((PINB & _BV(TM1637_DIO_PIN)) > 0) ? 1 : 0)
#define	TM1637_CLK_HIGH()		(PORTB |= _BV(TM1637_CLK_PIN))
#define	TM1637_CLK_LOW()		(PORTB &= ~_BV(TM1637_CLK_PIN))

// Main Settings
#define	TM1637_DIO_PIN			PB0
#define	TM1637_CLK_PIN			PB1
#define	TM1637_BRIGHTNESS_MAX		(7)
#define	TM1637_POSITION_MAX		(4)

// TM1637 commands
#define	TM1637_CMD_SET_DATA		0x40
#define	TM1637_CMD_SET_ADDR		0xC0
#define	TM1637_CMD_SET_DSIPLAY		0x80

// TM1637 data settings (use bitwise OR to contruct complete command)
#define	TM1637_SET_DATA_WRITE		0x00 // write data to the display register
#define	TM1637_SET_DATA_READ		0x02 // read the key scan data
#define	TM1637_SET_DATA_A_ADDR		0x00 // automatic address increment
#define	TM1637_SET_DATA_F_ADDR		0x04 // fixed address
#define	TM1637_SET_DATA_M_NORM		0x00 // normal mode
#define	TM1637_SET_DATA_M_TEST		0x10 // test mode

// TM1637 display control command set (use bitwise OR to consruct complete command)
#define	TM1637_SET_DISPLAY_OFF		0x00 // off
#define	TM1637_SET_DISPLAY_ON		0x08 // on

static void TM1637_send_config(const uint8_t enable, const uint8_t brightness);
static void TM1637_send_command(const uint8_t value);
static void TM1637_start(void);
static void TM1637_stop(void);
static uint8_t TM1637_write_byte(uint8_t value);

static uint8_t _config = TM1637_SET_DISPLAY_ON | TM1637_BRIGHTNESS_MAX;
static uint8_t _segments = 0xff;

// reads ADC on LM35 or reference from pullup diode
static int LM35_read(uint8_t);



/**
 * Initialize TM1637 display driver.
 * Clock pin (TM1637_CLK_PIN) and data pin (TM1637_DIO_PIN)
 * are defined at the top of this file.
 */
void TM1637_init(const uint8_t enable, const uint8_t brightness);

/**
 * Turn display on/off.
 * value: 1 - on, 0 - off
 */
void TM1637_enable(const uint8_t value);

/**
 * Set display brightness.
 * Min value: 0
 * Max value: 7
 */
void TM1637_set_brightness(const uint8_t value);

/**
 * Display raw segments at position (0x00..0x03)
 *
 *      bits:
 *        -- 0 --
 *       |       |
 *       5       1
 *       |       |
 *        -- 6 --
 *       |       |
 *       4       2
 *       |       |
 *        -- 3 --
 *
 * Example segment configurations:
 * - for character 'H', segments=0b01110110
 * - for character '-', segments=0b01000000
 * - etc.
 */
void TM1637_display_segments(const uint8_t position, const uint8_t segments);

/**
 * Display digit ('0'..'9') at position (0x00..0x03)
 */
void TM1637_display_digit(const uint8_t position, const uint8_t digit);

/**
 * Display colon on/off.
 * value: 1 - on, 0 - off
 */
void TM1637_display_colon(const uint8_t value);

/**
 * Clear all segments (including colon).
 */
void TM1637_clear(void);


PROGMEM const uint8_t _digit2segments[] =
{
	0x3F, // 0
	0x06, // 1
	0x5B, // 2
	0x4F, // 3
	0x66, // 4
	0x6D, // 5
	0x7D, // 6
	0x07, // 7
	0x7F, // 8
	0x6F  // 9
};


// to shorten the code to fit in ATTINY13
void _delay_1s(void)
{
__asm volatile (
    "    ldi  r18, 7"	"\n"
    "    ldi  r19, 23"	"\n"
    "    ldi  r20, 107"	"\n"
    "1:  dec  r20"	"\n"
    "    brne 1b"	"\n"
    "    dec  r19"	"\n"
    "    brne 1b"	"\n"
    "    dec  r18"	"\n"
    "    brne 1b"	"\n"
    "    nop"	"\n"
           );
};


int main(void)
{
	int temp, temp2;
        uint8_t firstdig;
        uint8_t seconddig;
        uint8_t thirddig;

	/* setup */
	TM1637_init(1/*enable*/, 5/*brightness*/);

	/* setup */
	DDRB &= ~_BV(LM35_DATA_PIN); // set data pin as INPUT
	DDRB &= ~_BV(LM35_GND_PIN); // set data pin as INPUT

	/* loop */
	while (1) {
       
        // read ADC value 16 times and compute mean for accuracy 
       temp = 0;
       temp2 = 0;
       for(uint8_t counts = 0; counts < 16; counts++) 
      {  // reading of Vout pin of LM35 ( temperature + V on Diode 1N4007)
        temp = temp + LM35_read(1);
       // reading value of voltage on LM35 GND pin ( V on Diode 1N4007 )for reference
        temp2 = temp2 + LM35_read(0);
      } ;
       temp = temp >> 4;
       temp2 = temp2 >> 4;

        // 
	//  we have to calculate if temperature is below 0 Celsius Degrees


		if (temp > temp2) {
			temp = temp - temp2;
                // if LM35 Vout is higher than V Diode 1N4007 - positive temp put DEGREES sign
                        TM1637_display_segments(3, 99);

		} else 
                  {
               //  if V on Diode is higher than Vout of LM35 - negative temp - put MINUS sign
                        TM1637_display_segments(3, 64);
                        temp = temp2 - temp;
		  }
             // calculation of digit values
               firstdig = (temp / 100);
               seconddig = (temp % 100) / 10 ; 
               thirddig = temp % 10;
        // put digits to display in correct order
                TM1637_display_digit(0, firstdig);
                TM1637_display_digit(1, seconddig);
                TM1637_display_digit(2, thirddig);
  	// wait a second
                _delay_1s();
	}
	/* loop */

}


int
LM35_read(uint8_t port)
{
	int temp;
	uint8_t low, high;

        if (port == 1) ADMUX = _BV(MUX1);    // ADC2 as input for LM35 Vout
        else ADMUX = _BV(MUX0)|_BV(MUX1);   // ADC3 to substract Diode voltage from Vout LM35

	ADMUX &= ~_BV(REFS0); // explicit set VCC as reference voltage (5V)
	ADCSRA |= _BV(ADEN);  // Enable ADC
	ADCSRA |= _BV(ADSC);  // Run single conversion

	while(bit_is_set(ADCSRA, ADSC)); // Wait conversion is done

	// Read values
	low = ADCL;
        high = ADCH;

        // combine two bytes
        temp =  (high << 8) | low;
        // convert for appropriate Vref   
	temp = ((((uint32_t)temp * 1000UL) >> 10) * 5); // convert value using euqation temp = Vin * 1000 / 1024 * Vref [milivolts]
	return temp;
}



void
TM1637_init(const uint8_t enable, const uint8_t brightness)
{

	DDRB |= (_BV(TM1637_DIO_PIN)|_BV(TM1637_CLK_PIN));
	PORTB &= ~(_BV(TM1637_DIO_PIN)|_BV(TM1637_CLK_PIN));
	TM1637_send_config(enable, brightness);
}

void
TM1637_enable(const uint8_t value)
{

	TM1637_send_config(value, _config & TM1637_BRIGHTNESS_MAX);
}

void
TM1637_set_brightness(const uint8_t value)
{

	TM1637_send_config(_config & TM1637_SET_DISPLAY_ON,
		value & TM1637_BRIGHTNESS_MAX);
}

void
TM1637_display_segments(const uint8_t position, const uint8_t segments)
{

	TM1637_send_command(TM1637_CMD_SET_DATA | TM1637_SET_DATA_F_ADDR);
	TM1637_start();
	TM1637_write_byte(TM1637_CMD_SET_ADDR | (position & (TM1637_POSITION_MAX - 1)));
	TM1637_write_byte(segments);
	TM1637_stop();
}

void
TM1637_display_digit(const uint8_t position, const uint8_t digit)
{
	uint8_t segments = (digit < 10 ? pgm_read_byte_near((uint8_t *)&_digit2segments + digit) : 0x00);

	if (position == 0x01) {
		segments = segments | (_segments & 0x80);
		_segments = segments;
	}

	TM1637_display_segments(position, segments);
}

void
TM1637_display_colon(const uint8_t value)
{
	if (value) {
		_segments |= 0x80;
	} else {
		_segments &= ~0x80;
	}
	TM1637_display_segments(0x01, _segments);
}

void
TM1637_clear(void)
{
	uint8_t i;

	for (i = 0; i < TM1637_POSITION_MAX; ++i) {
		TM1637_display_segments(i, 0x00);
	}
}

void
TM1637_send_config(const uint8_t enable, const uint8_t brightness)
{

	_config = (enable ? TM1637_SET_DISPLAY_ON : TM1637_SET_DISPLAY_OFF) |
		(brightness > TM1637_BRIGHTNESS_MAX ? TM1637_BRIGHTNESS_MAX : brightness);

	TM1637_send_command(TM1637_CMD_SET_DSIPLAY | _config);
}

void
TM1637_send_command(const uint8_t value)
{

	TM1637_start();
	TM1637_write_byte(value);
	TM1637_stop();
}

void
TM1637_start(void)
{

	TM1637_DIO_HIGH();
	TM1637_CLK_HIGH();

     // this is 5 microsecond delay
    __asm volatile (
    "    lpm"	"\n"
    "    lpm"	"\n"
            );
	TM1637_DIO_LOW();
}

void
TM1637_stop(void)
{

	TM1637_CLK_LOW();

     // this is 5 microsecond delay
   __asm volatile (
    "    lpm"	"\n"
    "    lpm"	"\n"
            );

	TM1637_DIO_LOW();
     // this is 5 microsecond delay
    __asm volatile (
    "    lpm"	"\n"
    "    lpm"	"\n"
            );

	TM1637_CLK_HIGH();

     // this is 5 microsecond delay
    __asm volatile (
    "    lpm"	"\n"
    "    lpm"	"\n"
            );

	TM1637_DIO_HIGH();
}

uint8_t
TM1637_write_byte(uint8_t value)
{
	uint8_t i, ack;

	for (i = 0; i < 8; ++i, value >>= 1) {
		TM1637_CLK_LOW();
 
     // this is 5 microsecond delay
    __asm volatile (
    "    lpm"	"\n"
    "    lpm"	"\n"
            );

		if (value & 0x01) {
			TM1637_DIO_HIGH();
		} else {
			TM1637_DIO_LOW();
		}

		TM1637_CLK_HIGH();
     // this is 5 microsecond delay
   __asm volatile (
    "    lpm"	"\n"
    "    lpm"	"\n"
            );	}

	TM1637_CLK_LOW();
	TM1637_DIO_INPUT();
	TM1637_DIO_HIGH();

     // this is 5 microsecond delay
   __asm volatile (
    "    lpm"	"\n"
    "    lpm"	"\n"
            );

	ack = TM1637_DIO_READ();
	if (ack) {
		TM1637_DIO_OUTPUT();
		TM1637_DIO_LOW();
	}
     // this is 5 microsecond delay
   __asm volatile (
    "    lpm"	"\n"
    "    lpm"	"\n"
            );
	TM1637_CLK_HIGH();

     // this is 5 microsecond delay
  __asm volatile (
    "    lpm"	"\n"
    "    lpm"	"\n"
            );
	TM1637_CLK_LOW();
 
     // this is 5 microsecond delay
  __asm volatile (
    "    lpm"	"\n"
    "    lpm"	"\n"
            );
	TM1637_DIO_OUTPUT();

	return ack;
}
