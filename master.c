#include <avr/io.h>
#include <util/delay.h>
#include <LCD.h>


int int_to_str(uint8_t num, char* str) {
    int i = 0, j, length;
    char temp[4];

    do {
        temp[i] = (num % 10) + '0';
        num /= 10;
        i++;
    } while (num);

    length = i;
    
    for (j = 0; i > 0; j++)
        str[j] = temp[--i];

    str[j] = '\0';

    return length;
}

void display_on_lcd(char* msg, int length, int clear_screen) {
    if (clear_screen) LCD_cmd(0x01);

    for (int i = 0; i < length; i++)
        LCD_write(msg[i]);
}

uint8_t get_temperature(void) {
    ADCSRA |= (1 << ADSC);  // Start conversion

    while ((ADCSRA & (1 << ADIF)) == 0);  // Wait till end of the conversion

    // ADC reference  -> Vref = 5v
    // Bit resolution -> n = 10
    // Max output     -> 2^n - 1 = 1023
    // Step size      -> Vref / (2^n - 1) = 4.887mv
    // LM35 provides 10mv voltage change for every celsius degree. So, 1 step -> nearly 5mv = 0.5'c
    return (uint8_t) (((uint16_t) ADCW) / (0.01 / (5.0 / 1023.0)));
}

void send_temperature(uint8_t temperature) {
        PORTB &= ~(1 << PORT4);  // Select slave

        SPDR = temperature;      // Send temperature to slave
        
        while(((SPSR >> SPIF) & 1) == 0); // Wait till the data are sent

        PORTB |= (1 << PORTB4);  // Deselect slave
}


int main(void) {
    /***** SPI initializations *****/

    // (SCK/DDB7 = 1)        -> Send clock
    // (MISO/DDB6 = 0)       -> Master in slave out
    // (MOSI/DDB5 = 1)       -> Master out slave in
    // (SS/DDB4 = 1)         -> Slave select
    DDRB = (1 << DDB7) | (0 << DDB6) | (1 << DDB5) | (1 << DDB4);
    PORTB = (1 << PORTB4);

    // (SPE = 1)             -> Enable SPI
    // (DORD = 0)            -> SPI data order: MSB First
    // (MSTR = 1)            -> SPI type: Master
    // (CPOL = 0)            -> SPI clock polarity: Low
    // (CPHA = 0)            -> SPI clock phase: Cycle Half
    // (SPI2X+SPR1..0 = 011) -> SPI clock rate: 8MHz / 128 = 62.5 kHz
    SPCR = (1 << SPE) | (0 << DORD) | (1 << MSTR) | (0 << CPOL) | (0 << CPHA) | (1 << SPR1) | (1 << SPR0);
    SPSR = (0 << SPI2X);

    /***** ADC initializations *****/
    
    // (REFS1..0 = 01)       -> Vref = AVcc = 5v
    // (ADLAR = 0)           -> Result right justified (Start from right in ADCL)
    // (MUX4..0 = 00000)     -> Single ended input ADC0
    ADMUX = (0 << REFS1) | (1 << REFS0);
    // (ADEN = 1)            -> ADC enable
    // (ADPS2..0 = 111)      -> Pre-scale with division factor 128
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);

    /***** AC initializations *****/

    // (ACME = 0)            -> V+ comes from AIN0 (Disable AC multiplexer)
    SFIOR = (0 << ACME);
    // (ACD = 0)             -> Enable AC
    // (ACBG = 0)            -> V- comes from AIN1 (Disable AC band-gap)
    // (ACO = 0)             -> Set initial AC output
    ACSR = (0 << ACD) | (0 << ACBG) | (0 << ACO);

    /***** LCD initializations *****/

    DDRC = 0xFF;  // All pins in port C are outputs
    DDRD = 0x07;  // First 3 pins in port D are outputs
    
    init_LCD();

    _delay_ms(500);

    uint8_t old_temperature = 0, new_temperature;

    while (1) {
        if (((ACSR >> ACO) & 1) == 1) {  // If sensor A voltage > sensor B voltage
            new_temperature = get_temperature();
            send_temperature(new_temperature);
            
            if (new_temperature != old_temperature) {
                if ((new_temperature < 0) || (new_temperature > 100))
                    display_on_lcd("Out of range", 12, 1);
                else {
                    char str_temperature[4];
                    int length = int_to_str(new_temperature, str_temperature);

                    display_on_lcd("Temperature: ", 13, 1);
                    display_on_lcd(str_temperature, length, 0);
                }
                
                old_temperature = new_temperature;
            }
        } else {
            new_temperature = 255;  // Something out of our coverage range (0-100)
            
            if (new_temperature != old_temperature) {
                display_on_lcd("      A < B", 11, 1);

                send_temperature(new_temperature);
                old_temperature = new_temperature;
            }
        }
        
        _delay_ms(500);
    }

    return 0;
}
