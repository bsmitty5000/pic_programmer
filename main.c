#include <p24FJ64GA002.h>

#define DELAY 15625

int main () {

    TRISB = 0x0000;
    T1CON = 0x8030;
    
       
    while(1) {

        LATB = 0xFFFF;
        TMR1 = 0;
        while (TMR1 < DELAY);

        LATB = 0x0000;
        TMR1 = 0;
        while (TMR1 < DELAY);

    }

    return 1;
}

