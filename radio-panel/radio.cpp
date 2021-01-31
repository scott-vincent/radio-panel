#include <stdio.h>
#include <stdlib.h>
#include "gpioctrl.h"
#include "radio.h"

radio::radio()
{
    simVars = &globals.simVars->simVars;
    addGpio();

    // Initialise 7-segment displays
    sevenSegment = new sevensegment(false, 0);

    fflush(stdout);
}

void radio::render()
{
    if (!globals.electrics) {
        // Turn off 7-segment displays
        sevenSegment->blankSegData(display1, 8, false);
        sevenSegment->blankSegData(display2, 8, false);
        sevenSegment->blankSegData(display3, 8, false);
        sevenSegment->writeSegData3(display1, display2, display3);

        // Turn off LEDS
        globals.gpioCtrl->writeLed(comControl, false);
        globals.gpioCtrl->writeLed(navControl, false);

        // Make sure settings get re-initialised
        loadedAircraft = UNDEFINED;

        return;
    }

    // Active frequency
    if (showNav) {
        writeNav(display1, activeFreq);
    }
    else {
        writeCom(display1, activeFreq);
    }

    // Standby frequency
    if (showNav) {
        writeNav(display2, standbyFreq);
    }
    else {
        writeCom(display2, standbyFreq);
    }

    // Transponder code is in BCO16
    int code = squawk;
    int digit1 = code / 4096;
    code -= digit1 * 4096;
    int digit2 = code / 256;
    code -= digit2 * 256;
    int digit3 = code / 16;
    int digit4 = code - digit3 * 16;
    code = digit1 * 1000 + digit2 * 100 + digit3 * 10 + digit4;
    sevenSegment->getSegData(&display3[2], 4, code, 4);
    sevenSegment->blankSegData(display3, 2, false);
    sevenSegment->blankSegData(&display3[6], 2, false);

    // If squawk is being adjusted show a dot to indicate which digit
    if (lastSquawkAdjust != 0) {
        sevenSegment->decimalSegData(display3, 2 + squawkSetSel);
    }

    // Write to 7-segment displays
    sevenSegment->writeSegData3(display1, display2, display3);

    // Write LEDs
    globals.gpioCtrl->writeLed(comControl, !showNav);
    globals.gpioCtrl->writeLed(navControl, showNav);
}

/// <summary>
/// Com is 6 digits (3 whole and 3 frac) but display is 8 digits
/// so leave one blank on left and one on right.
/// </summary>
void radio::writeCom(unsigned char* display, double freq)
{
    int freqX1000 = (freq + 0.0001) * 1000;
    int whole = freqX1000 / 1000;
    int frac = freqX1000 % 1000;

    sevenSegment->blankSegData(display, 1, false);
    sevenSegment->getSegData(&display[1], 3, whole, 3);
    sevenSegment->decimalSegData(display, 3);
    sevenSegment->getSegData(&display[4], 3, frac, 3);
    sevenSegment->blankSegData(&display[7], 1, false);
}

/// <summary>
/// Nav is 5 digits (3 whole and 2 frac) but display is 8 digits
/// so leave one blank on left and two on right.
/// </summary>
void radio::writeNav(unsigned char* display, double freq)
{
    int freqX100 = (freq + 0.001) * 100;
    int whole = freqX100 / 100;
    int frac = freqX100 % 100;

    sevenSegment->blankSegData(display, 1, false);
    sevenSegment->getSegData(&display[1], 3, whole, 3);
    sevenSegment->decimalSegData(display, 3);
    sevenSegment->getSegData(&display[4], 2, frac, 2);
    sevenSegment->blankSegData(&display[6], 2, false);
}

void radio::update()
{
    // Check for aircraft change
    bool aircraftChanged = (loadedAircraft != globals.aircraft);
    if (aircraftChanged) {
        loadedAircraft = globals.aircraft;
        lastFreqAdjust = 0;
        lastSquawkAdjust = 0;
        setFreqFrac = 0;
        setSquawk = 0;
        showNav = false;
    }

    gpioFreqWholeInput();
    gpioFreqFracInput();
    gpioButtonsInput();
    gpioSquawkInput();

    // Only update local values from sim if they are not currently being
    // adjusted by the rotary encoders. This stops the displayed values
    // from jumping around due to lag of fetch/update cycle.
    if (showNav) {
        activeFreq = simVars->nav1Freq;
        if (lastFreqAdjust == 0) {
            standbyFreq = simVars->nav1Standby;
        }
    }
    else {
        activeFreq = simVars->com1Freq;
        if (lastFreqAdjust == 0) {
            standbyFreq = simVars->com1Standby;
        }
    }

    if (lastSquawkAdjust == 0) {
        squawk = simVars->transponderCode;
    }
}

void radio::addGpio()
{
    freqWholeControl = globals.gpioCtrl->addRotaryEncoder("Frequency Whole");
    freqFracControl = globals.gpioCtrl->addRotaryEncoder("Frequency Fraction");
    swapControl = globals.gpioCtrl->addButton("Swap");
    comControl = globals.gpioCtrl->addButton("Com");
    navControl = globals.gpioCtrl->addButton("Nav");
    squawkControl = globals.gpioCtrl->addRotaryEncoder("Squawk");
}

void radio::gpioFreqWholeInput()
{
    // Frequency whole rotate
    int val = globals.gpioCtrl->readRotation(freqWholeControl);
    if (val != INT_MIN) {
        int diff = (val - prevFreqWholeVal) / 4;
        int adjust = 0;
        if (diff > 0) {
            adjust = 1;
        }
        else if (diff < 0) {
            adjust = -1;
        }

        if (adjust != 0) {
            // Adjust frequency
            if (showNav) {
                double newVal = adjustNavWhole(adjust);
                globals.simVars->write(KEY_NAV1_STBY_SET, newVal);
            }
            else {
                double newVal = adjustComWhole(adjust);
                globals.simVars->write(KEY_COM1_STBY_RADIO_SET, newVal);
            }
            prevFreqWholeVal = val;
        }
        time(&lastFreqAdjust);      // Gets reset by frac input
    }
}

void radio::gpioFreqFracInput()
{
    // Frequency fraction rotate
    int val = globals.gpioCtrl->readRotation(freqFracControl);
    if (val != INT_MIN) {
        int diff = (val - prevFreqFracVal) / 4;
        int adjust = 0;
        if (diff > 0) {
            adjust = 1;
        }
        else if (diff < 0) {
            adjust = -1;
        }

        if (adjust != 0) {
            // Adjust frequency
            if (showNav) {
                double newVal = adjustNavFrac(adjust);
                globals.simVars->write(KEY_NAV1_STBY_SET, newVal);
            }
            else {
                double newVal = adjustComFrac(adjust);
                globals.simVars->write(KEY_COM1_STBY_RADIO_SET, newVal);
            }
            prevFreqFracVal = val;
        }
        time(&lastFreqAdjust);
    }
    else if (lastFreqAdjust != 0) {
        // Reset digit set selection if more than 2 seconds since last adjustment
        time(&now);
        if (now - lastFreqAdjust > 2) {
            fracSetSel = 0;
            lastFreqAdjust = 0;
        }
    }

    // Frequency fraction push
    val = globals.gpioCtrl->readPush(freqFracControl);
    if (val != INT_MIN) {
        if (prevFreqFracPush % 2 == 1) {
            // Short press switches between 10s and 100ths frequency increments
            if (fracSetSel == 1) {
                fracSetSel = 0;
            }
            else {
                fracSetSel++;
            }
        }
        prevFreqFracPush = val;
    }
}

void radio::gpioButtonsInput()
{
    // Swap push
    int val = globals.gpioCtrl->readPush(swapControl);
    if (val != INT_MIN) {
        if (prevSwapPush % 2 == 1) {
            // Swap active and standby frequencies
            if (showNav) {
                globals.simVars->write(KEY_NAV1_RADIO_SWAP);
            }
            else {
                globals.simVars->write(KEY_COM1_RADIO_SWAP);
            }
            lastFreqAdjust = 0;
        }
        prevSwapPush = val;
    }

    // Com push
    val = globals.gpioCtrl->readPush(comControl);
    if (val != INT_MIN) {
        if (prevComPush % 2 == 1) {
            // Show Com1
            showNav = false;
        }
        fracSetSel = 0;
        lastFreqAdjust = 0;
        prevComPush = val;
    }

    // Nav push
    val = globals.gpioCtrl->readPush(navControl);
    if (val != INT_MIN) {
        if (prevNavPush % 2 == 1) {
            // Show Nav1
            showNav = true;
        }
        fracSetSel = 0;
        lastFreqAdjust = 0;
        prevNavPush = val;
    }
}

void radio::gpioSquawkInput()
{
    // Squawk rotate
    int val = globals.gpioCtrl->readRotation(squawkControl);
    if (val != INT_MIN) {
        int diff = (val - prevSquawkVal) / 4;
        int adjust = 0;
        if (diff > 0) {
            adjust = 1;
        }
        else if (diff < 0) {
            adjust = -1;
        }

        if (adjust != 0) {
            // Adjust squawk
            int newVal = adjustSquawk(adjust);
            globals.simVars->write(KEY_XPNDR_SET, newVal);
            prevSquawkVal = val;
        }
        time(&lastSquawkAdjust);
    }
    else if (lastSquawkAdjust != 0) {
        // Reset digit set selection if more than 2 seconds since last adjustment
        time(&now);
        if (now - lastSquawkAdjust > 2) {
            squawkSetSel = 0;
            lastSquawkAdjust = 0;
        }
    }

    // Squawk push
    val = globals.gpioCtrl->readPush(squawkControl);
    if (val != INT_MIN) {
        if (prevSquawkPush % 2 == 1) {
            // Short press switches to next digit
            if (squawkSetSel == 3) {
                squawkSetSel = 0;
            }
            else {
                squawkSetSel++;
            }
        }
        prevSquawkPush = val;
        time(&lastSquawkAdjust);
    }
}

double radio::adjustComWhole(int adjust)
{
    int whole = standbyFreq;
    double val  = standbyFreq - whole;
    int thousandths = (val + 0.0001) * 1000;
    int frac1 = thousandths / 100;
    int frac2 = thousandths % 100;

    // Adjust whole - Range 118 to 136
    whole += adjust;
    if (whole > 136) {
        whole = 118;
    }
    else if (whole < 118) {
        whole = 136;
    }

    standbyFreq = whole + (thousandths / 1000.0);

    // Convert to BCD
    int digit1 = whole / 100;
    int digit2 = (whole % 100) / 10;
    int digit3 = whole % 10;
    int digit4 = frac1;
    int digit5 = frac2 / 10;
    int digit6 = frac2 % 10;

    // Return digit6 as fraction
    return 65536 * digit1 + 4096 * digit2 + 256 * digit3 + 16 * digit4 + digit5 + digit6 * 0.1;
}

double radio::adjustComFrac(int adjust)
{
    int whole = standbyFreq;
    double val = standbyFreq - whole;
    int thousandths = (val + 0.0001) * 1000;
    int frac1 = thousandths / 100;
    int frac2 = thousandths % 100;

    if (fracSetSel == 0) {
        // Adjust 10ths
        frac1 += adjust;
        if (frac1 > 9) {
            frac1 = 0;
        }
        else if (frac1 < 0) {
            frac1 = 9;
        }
    }
    else {
        // Adjust 100ths and 1000ths
        frac2 += adjust * 5;

        if (frac2 >= 100) {
            frac2 = 0;
        }
        else if (frac2 < 0) {
            frac2 = 90;
        }

        // Skip .020, .045, .070 and .095
        if (frac2 == 95) {
            frac2 += adjust * 5;
            if (frac2 >= 100) {
                frac2 = 0;
            }
        }
        else if (frac2 == 20 || frac2 == 45 || frac2 == 70) {
            frac2 += adjust * 5;
        }
    }

    standbyFreq = whole + (frac1 / 10.0) + (frac2 / 1000.0);

    // Need to set .020 to show .025 and .070 to show .075 !!!
    if (frac2 == 25 || frac2 == 75) {
        frac2 -= 5;
    }

    // Convert to BCD
    int digit1 = whole / 100;
    int digit2 = (whole % 100) / 10;
    int digit3 = whole % 10;
    int digit4 = frac1;
    int digit5 = frac2 / 10;
    int digit6 = frac2 % 10;

    // Return digit6 as fraction
    return 65536 * digit1 + 4096 * digit2 + 256 * digit3 + 16 * digit4 + digit5 + digit6 * 0.1;
}

double radio::adjustNavWhole(int adjust)
{
    int whole = standbyFreq;
    double val = standbyFreq - whole;
    int frac = (val + 0.001) * 100;

    // Adjust whole - Range 108 to 117
    whole += adjust;
    if (whole > 117) {
        whole = 108;
    }
    else if (whole < 108) {
        whole = 117;
    }

    standbyFreq = whole + (frac / 100.0);

    // Convert to BCD
    int digit1 = whole / 100;
    int digit2 = (whole % 100) / 10;
    int digit3 = whole % 10;
    int digit4 = frac / 10;
    int digit5 = frac % 10;

    return 65536 * digit1 + 4096 * digit2 + 256 * digit3 + 16 * digit4 + digit5;
}

double radio::adjustNavFrac(int adjust)
{
    int whole = standbyFreq;
    double val = standbyFreq - whole;
    int frac = (val + 0.001) * 100;

    // Adjust fraction
    frac += adjust * 5;

    if (frac >= 100) {
        frac = 0;
    }
    else if (frac < 0) {
        frac = 95;
    }

    standbyFreq = whole + (frac / 100.0);

    // Convert to BCD
    int digit1 = whole / 100;
    int digit2 = (whole % 100) / 10;
    int digit3 = whole % 10;
    int digit4 = frac / 10;
    int digit5 = frac % 10;

    return 65536 * digit1 + 4096 * digit2 + 256 * digit3 + 16 * digit4 + digit5;
}

int radio::adjustSquawk(int adjust)
{
    // Transponder code is in BCD
    int code = squawk;
    int digit1 = code / 4096;
    code -= digit1 * 4096;
    int digit2 = code / 256;
    code -= digit2 * 256;
    int digit3 = code / 16;
    int digit4 = code - digit3 * 16;

    switch (squawkSetSel) {
        case 0:
            digit1 = adjustSquawkDigit(digit1, adjust);
            break;
        case 1:
            digit2 = adjustSquawkDigit(digit2, adjust);
            break;
        case 2:
            digit3 = adjustSquawkDigit(digit3, adjust);
            break;
        case 3:
            digit4 = adjustSquawkDigit(digit4, adjust);
            break;
    }

    // Convert to BCD
    squawk = digit1 * 4096 + digit2 * 256 + digit3 * 16 + digit4;
    return squawk;
}

int radio::adjustSquawkDigit(int val, int adjust)
{
    val += adjust;
    if (val > 7) {
        val = 0;
    }
    else if (val < 0) {
        val = 7;
    }

    return val;
}
