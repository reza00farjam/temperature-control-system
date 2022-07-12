#include <avr/io.h>


uint8_t get_duty_cycle(uint8_t temperature) {
	if ((temperature >= 25) && (temperature < 30)) return 50;
	if ((temperature >= 30 ) && (temperature < 35)) return 60;
	if ((temperature >= 35 ) && (temperature < 40)) return 70;
	if ((temperature >= 40 ) && (temperature < 45)) return 80;
	if ((temperature >= 45 ) && (temperature < 50)) return 90;
	if ((temperature >= 50 ) && (temperature <= 55)) return 100;
	return 0;
}


int main() {
    /***** LED and Heater initializations *****/

    DDRC = (1 << DDC1) | (1 << DDC0);

    /***** SPI initializations *****/

    // (SCK/DDB7 = 0)        -> Clock
    // (MISO/DDB6 = 1)       -> Master in slave out
    // (MOSI/DDB5 = 0)       -> Master out slave in
    // (SS/DDB4 = 0)         -> Being slave
    DDRB = (0 << DDB7) | (1 << DDB6) | (0 << DDB5) | (0 << DDB4);

    // (SPE = 1)             -> Enable SPI
    // (DORD = 0)            -> SPI data order: MSB First
    // (MSTR = 0)            -> SPI type: Slave
    // (CPOL = 0)            -> SPI clock polarity: Low
    // (CPHA = 0)            -> SPI clock phase: Cycle Half
    // (SPI2X+SPR1..0 = 011) -> SPI clock rate: 8MHz / 128 = 62.5 kHz
    SPCR = (1 << SPE) | (0 << DORD) | (0 << MSTR) | (0 << CPOL) | (0 << CPHA) | (1 << SPR1) | (1 << SPR0);
    SPSR = (0 << SPI2X);
    SPDR = 0x00;

    /***** Cooler initializations *****/

    // (WG01..00 = 11)       -> Enable Fast PWM
    // (COM01..00 = 10)      -> Clear OC0 on compare match (non-inverted)
    // (CS02..00 = 010)      -> Pre-scale 8
    TCCR0 = (1 << WGM00) | (1 << COM01) | (0 << COM00) | (1 << WGM01) | (0 << CS02) | (1 << CS01) | (0 << CS00);
    DDRB |= (1 << DDB3);  // Enable OC0 pin

    uint8_t old_duty_cycle = 0;

    while(1) {
        while (((SPSR >> SPIF) & 1) == 0);  // Wait till the data are received

        uint8_t temperature = SPDR;
        uint8_t new_duty_cycle = get_duty_cycle(temperature);

        if (new_duty_cycle != old_duty_cycle) {
            OCR0 = (new_duty_cycle / 100.0) * 255.0;  // 255: OCR0 max value
            old_duty_cycle = new_duty_cycle;
        }

        if ((temperature > 55) && (temperature <= 100)) PORTC ^= (1 << PORTC0); // Toggle LED (Make it blink!)
        else PORTC &= ~(1 << PORTC0);  // Turn off LED

        if ((temperature >= 0) && (temperature < 20)) PORTC |= (1 << PORTC1); // Turn on heater
        else PORTC &= ~(1 << PORTC1); // Turn off heater
    }
}
