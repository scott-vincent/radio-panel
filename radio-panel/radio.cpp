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

void radio::blankDisplays()
{
    sevenSegment->blankSegData(display1, 8, false);
    sevenSegment->blankSegData(display2, 8, false);
    sevenSegment->blankSegData(display3, 8, false);
    sevenSegment->writeSegData3(display1, display2, display3);
}

void radio::render()
{
    if (!globals.electrics) {
        // Turn off 7-segment displays
        blankDisplays();

        // Turn off LEDS
        globals.gpioCtrl->writeLed(comControl, false);
        globals.gpioCtrl->writeLed(navControl, false);
        globals.gpioCtrl->writeLed(seatBeltsControl, false);

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
        if (lastFreqAdjust != 0 && fracSetSel == 1) {
            sevenSegment->decimalSegData(display2, 6);
        }
    }

    // Dim the transponder display if it is not active
    //if (transponderState != simVars->transponderState) {
    //    transponderState = simVars->transponderState;
    //    if (transponderState < 3) {
    //        sevenSegment->dimDisplay(1, true);
    //    }
    //    else {
    //        sevenSegment->dimDisplay(1, false);
    //    }
    //}

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

    // Seat Belts sign
    globals.gpioCtrl->writeLed(seatBeltsControl, showSeatBelts);
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
    bool aircraftChanged = (globals.electrics && loadedAircraft != globals.aircraft);
    if (aircraftChanged) {
        loadedAircraft = globals.aircraft;
        lastFreqAdjust = 0;
        lastSquawkAdjust = 0;
        squawk = 0;
        transponderState = -1;
        showNav = false;
        lastSpoilersPos = -1;
        prevSpoilersAutoToggle = -1;
        prevSpoilersDownToggle = -1;
        prevGearUpToggle = -1;
        prevGearDownToggle = -1;
    }

    time(&now);
    gpioFreqWholeInput();
    gpioFreqFracInput();
    gpioButtonsInput();
    gpioSquawkInput();
    gpioTrimWheelInput();
    gpioSpoilersInput();
    gpioGearInput();

    // Only update local values from sim if they are not currently being
    // adjusted by the rotary encoders. This stops the displayed values
    // from jumping around due to lag of fetch/update cycle.
    if (showNav) {
        activeFreq = simVars->nav1Freq;
        if (lastFreqAdjust == 0) {
            standbyFreq = simVars->nav1Standby;
        }
    }
    else if (simVars->pilotTransmitter == 0) {
        // Using COM1
        activeFreq = simVars->com1Freq;
        if (lastFreqAdjust == 0) {
            standbyFreq = simVars->com1Standby;
        }
    }
    else {
        // Using COM2
        activeFreq = simVars->com2Freq;
        if (lastFreqAdjust == 0) {
            standbyFreq = simVars->com2Standby;
        }
    }

    if (lastSquawkAdjust == 0) {
        if (loadedAircraft == BOEING_747 && squawk != 0) {
            // B747 Bug - Force squawk code back to set value
            if (simVars->transponderCode != squawk) {
                int newVal = adjustSquawk(0);
                globals.simVars->write(KEY_XPNDR_SET, newVal);
            }
        }
        else {
            squawk = simVars->transponderCode;
        }
    }

    // Seat Belts
    showSeatBelts = simVars->seatBeltsSwitch;
}

void radio::addGpio()
{
    freqWholeControl = globals.gpioCtrl->addRotaryEncoder("Frequency Whole");
    freqFracControl = globals.gpioCtrl->addRotaryEncoder("Frequency Fraction");
    swapControl = globals.gpioCtrl->addButton("Swap");
    comControl = globals.gpioCtrl->addButton("Com");
    navControl = globals.gpioCtrl->addButton("Nav");
    squawkControl = globals.gpioCtrl->addRotaryEncoder("Squawk");
    trimWheelControl = globals.gpioCtrl->addRotaryEncoder("Trim Wheel");
    spoilersAutoControl = globals.gpioCtrl->addSwitch("Spoilers Auto");
    spoilersPosControl = globals.gpioCtrl->addRotaryEncoder("Spoilers Pos");
    spoilersDownControl = globals.gpioCtrl->addSwitch("Spoilers Down");
    gearUpControl = globals.gpioCtrl->addSwitch("Gear Up");
    gearDownControl = globals.gpioCtrl->addSwitch("Gear Down");
    seatBeltsControl = globals.gpioCtrl->addLamp("Seat Belts");
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
            else if (simVars->pilotTransmitter == 0) {
                // Using COM1
                double newVal = adjustComWhole(adjust);
                globals.simVars->write(KEY_COM1_STBY_RADIO_SET, newVal);
            }
            else {
                // Using COM2
                double newVal = adjustComWhole(adjust);
                globals.simVars->write(KEY_COM2_STBY_RADIO_SET, newVal);
            }
            prevFreqWholeVal = val;
        }
        fracSetSel = 0;
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
            else if (simVars->pilotTransmitter == 0) {
                // Using COM1
                double newVal = adjustComFrac(adjust);
                globals.simVars->write(KEY_COM1_STBY_RADIO_SET, newVal);
            }
            else {
                // Using COM2
                double newVal = adjustComFrac(adjust);
                globals.simVars->write(KEY_COM2_STBY_RADIO_SET, newVal);
            }
            prevFreqFracVal = val;
        }
        time(&lastFreqAdjust);
    }
    else if (lastFreqAdjust != 0) {
        // Reset digit set selection if more than 2 seconds since last adjustment
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
                fracSetSel = 1;
            }
        }
        prevFreqFracPush = val;
        time(&lastFreqAdjust);
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
            else if (simVars->pilotTransmitter == 0) {
                // Using COM1
                globals.simVars->write(KEY_COM1_RADIO_SWAP);
            }
            else {
                // Using COM2
                globals.simVars->write(KEY_COM2_RADIO_SWAP);
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
            globals.gpioCtrl->writeLed(comControl, !showNav);
            globals.gpioCtrl->writeLed(navControl, showNav);
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
            globals.gpioCtrl->writeLed(comControl, !showNav);
            globals.gpioCtrl->writeLed(navControl, showNav);

            // If Com is also being pressed exit radio panel
            // and let it auto-restart (full reset).
            if (prevComPush % 2 == 0) {
                printf("Hard reset\n");
                blankDisplays();
                exit(1);
            }
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
            if (lastSquawkAdjust == 0 || squawkSetSel == 3) {
                squawkSetSel = 0;
            }
            else {
                squawkSetSel++;
            }
            time(&lastSquawkAdjust);
            time(&lastSquawkPush);
        }
        if (val % 2 == 1) {
            // Released
            lastSquawkPush = 0;
        }
        prevSquawkPush = val;
    }

    // Squawk long push (over 1 sec)
    if (lastSquawkPush > 0) {
        if (now - lastSquawkPush > 1) {
            // Long press switches transponder state between Standby and Alt
            // No SimConnect event to set transponder state yet!
            squawkSetSel = 0;
            lastSquawkAdjust = 0;
        }
    }
}

void radio::gpioTrimWheelInput()
{
    // Trim wheel rotate
    int val = globals.gpioCtrl->readRotation(trimWheelControl);
    if (val != INT_MIN) {
        int adjust = val - prevTrimWheelVal;

        // Adjust elevator trim
        while (adjust != 0) {
            if (adjust > 0) {
                globals.simVars->write(KEY_ELEV_TRIM_UP);
                adjust--;
            }
            else {
                globals.simVars->write(KEY_ELEV_TRIM_DN);
                adjust++;
            }
        }
        prevTrimWheelVal = val;
    }
}

void radio::gpioSpoilersInput()
{
    // Spoilers rotate
    int val = globals.gpioCtrl->readRotation(spoilersPosControl);
    if (val != INT_MIN) {
        spoilersVal = val;
    }

    // Spoilers auto toggle
    val = globals.gpioCtrl->readToggle(spoilersAutoControl);
    if (val != INT_MIN && val != prevSpoilersAutoToggle) {
        // Switch toggled
        prevSpoilersAutoToggle = val;
        if (val == 1) {
            // Switch pressed
            globals.simVars->write(KEY_SPOILERS_ARM_SET, 1);
            lastSpoilersPos = 0;
            if (spoilersVal != INT_MIN) {
                spoilersAutoVal = spoilersVal;  // Re-calibrate spoilers values
                int diff = spoilersDownVal - spoilersAutoVal;
                if (diff < 17 || diff > 23) {
                    spoilersDownVal = spoilersAutoVal + 20;
                }
            }
            return;
        }
    }

    // Spoilers down toggle
    val = globals.gpioCtrl->readToggle(spoilersDownControl);
    if (val != INT_MIN && val != prevSpoilersDownToggle) {
        // Switch toggled
        prevSpoilersDownToggle = val;
        if (val == 1) {
            // Switch pressed
            globals.simVars->write(KEY_SPOILERS_ON);
            lastSpoilersPos = 3;
            if (spoilersVal != INT_MIN) {
                spoilersDownVal = spoilersVal;    // Re-calibrate spoilers values
                int diff = spoilersDownVal - spoilersAutoVal;
                if (diff < 17 || diff > 23) {
                    spoilersAutoVal = spoilersDownVal - 20;
                }
            }
            return;
        }
    }

    if (spoilersVal != INT_MIN) {
        // Check for new spoilers position
        double onePos = (spoilersDownVal - spoilersAutoVal) / 3.0;
        int spoilersPos = (spoilersVal + (onePos / 2.0) - spoilersAutoVal) / onePos;
        if (spoilersPos != lastSpoilersPos) {
            lastSpoilersPos = spoilersPos;
            // Set spoilers to retracted or half
            switch (spoilersPos) {
            case 1:
                //globals.simVars->write(KEY_SPOILERS_ARM_SET, 0);
                globals.simVars->write(KEY_SPOILERS_OFF);
                break;
            case 2:
                globals.simVars->write(KEY_SPOILERS_ARM_SET, 0);
                globals.simVars->write(KEY_SPOILERS_SET, 8192);
                break;
            }
        }
    }
}

void radio::gpioGearInput()
{
    // Gear up toggle
    int val = globals.gpioCtrl->readToggle(gearUpControl);
    if (val != INT_MIN && val != prevGearUpToggle) {
        // Switch toggled
        if (val == 1 && gearDown) {
            // Switch pressed
            globals.simVars->write(KEY_GEAR_SET, 0);
            gearDown = false;
        }
        prevGearUpToggle = val;
    }

    // Gear down toggle
    val = globals.gpioCtrl->readToggle(gearDownControl);
    if (val != INT_MIN && val != prevGearDownToggle) {
        // Switch toggled
        if (val == 1 && !gearDown) {
            // Switch pressed
            globals.simVars->write(KEY_GEAR_SET, 1);
            gearDown = true;
        }
        prevGearDownToggle = val;
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
