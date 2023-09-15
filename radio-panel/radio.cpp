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
    if (!globals.electrics || (loadedAircraft == CESSNA_152 && simVars->com1Volume == 0 && simVars->com2Volume == 0)) {
        // Turn off 7-segment displays
        blankDisplays();

        // Turn off LEDS
        globals.gpioCtrl->writeLed(comControl, false);
        globals.gpioCtrl->writeLed(navControl, false);
        globals.gpioCtrl->writeLed(seatBeltsControl, false);

        if (!globals.electrics) {
            // Make sure settings get re-initialised
            loadedAircraft = UNDEFINED;
        }

        return;
    }

    // Active frequency
    if (showNav) {
        if (usingNav < 2) {
            writeNav(display1, activeFreq);
        }
        else {
            writeAdf(display1, activeFreq);
        }
    }
    else {
        writeCom(display1, activeFreq);

        if (receiveAllHideDelay > 0) {
            // Add dot on right if receiving on ALL
            if (simVars->com1Receive && simVars->com2Receive) {
                sevenSegment->decimalSegData(display1, 6);
            }
            receiveAllHideDelay--;
        }
    }

    // Standby frequency
    if (showNav) {
        if (usingNav < 2) {
            writeNav(display2, standbyFreq);
        }
        else {
            writeAdf(display2, standbyFreq);
        }
    }
    else {
        writeCom(display2, standbyFreq);

        if (lastFreqAdjust != 0 && fracSetSel == 1) {
            sevenSegment->decimalSegData(display2, 6);
        }
    }

    if (lastTcasAdjust == 0) {
        tcasMode = simVars->jbTcasMode;
        if (loadedAircraft != AIRBUS_A310) {
            transponderState = simVars->transponderState;
        }
    }

    // Dim the transponder display if it is not active
    if (tcasMode != lastTcasMode || transponderState != lastTransponderState) {
        lastTcasMode = tcasMode;
        lastTransponderState = transponderState;
        if (loadedAircraft == AIRBUS_A310 && tcasMode == 0) {
            sevenSegment->dimDisplay(1, true);
        }
        else if ((loadedAircraft == FBW_A320) && transponderState == 0) {
            sevenSegment->dimDisplay(1, true);
        }
        else if (!airliner && transponderState < 3) {
            // States 0, 1 and 2 are not on (off, stby, tst)
            sevenSegment->dimDisplay(1, true);
        }
        else {
            sevenSegment->dimDisplay(1, false);
        }
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

/// <summary>
/// Adf is 4 digits + .0 but display is 8 digits
/// so leave two blank on left and one on right.
/// Leading 0 is also shown as a blank.
/// </summary>
void radio::writeAdf(unsigned char* display, double freq)
{
    int freq1000 = freq / 100;
    int freq100 = int(freq) % 100;
    int freqX10 = (freq + 0.01) * 10;
    int frac = freqX10 % 10;

    if (freq1000 > 9) {
        sevenSegment->blankSegData(display, 2, false);
        sevenSegment->getSegData(&display[2], 2, freq1000, 2);
    }
    else {
        sevenSegment->blankSegData(display, 3, false);
        sevenSegment->getSegData(&display[3], 1, freq1000, 1);
    }

    sevenSegment->getSegData(&display[4], 2, freq100, 2);
    sevenSegment->decimalSegData(display, 5);
    sevenSegment->getSegData(&display[6], 1, frac, 1);
    sevenSegment->blankSegData(&display[7], 1, false);
}

void radio::update()
{
    // Check for aircraft change
    bool aircraftChanged = (globals.electrics && loadedAircraft != globals.aircraft);
    if (aircraftChanged) {
        loadedAircraft = globals.aircraft;
        airliner = (loadedAircraft != NO_AIRCRAFT && simVars->cruiseSpeed >= 300);
        lastFreqAdjust = 0;
        lastFreqPush = 0;
        lastSquawkAdjust = 0;
        lastSquawkPush = 0;
        squawk = 0;
        tcasMode = -1;
        transponderState = -1;
        showNav = false;
        usingNav = 0;
        lastSpoilersPos = -1;
        prevSpoilersAutoToggle = -1;
        prevSpoilersDownToggle = -1;
        prevGearUpToggle = -1;
        prevGearDownToggle = -1;
        prevFreqWholeValSb = simVars->sbEncoder[1];
        prevFreqFracValSb = simVars->sbEncoder[0];
        prevFreqFracPushSb = simVars->sbButton[0];
        prevSquawkValSb = simVars->sbEncoder[3];
        prevSquawkPushSb = simVars->sbButton[3];
        prevSwapPushSb = simVars->sbButton[6];
        prevComPushSb = simVars->sbButton[5];
        prevNavPushSb = simVars->sbButton[4];

        // Set COM2 to GUARD frequency and receive on ALL (needed for POSCON)
        standbyFreq = 121.5;
        globals.simVars->write(KEY_COM2_STBY_RADIO_SET, adjustComWhole(0));
        globals.simVars->write(KEY_COM2_RADIO_SWAP);
        globals.simVars->write(KEY_COM1_TRANSMIT_SELECT);
        globals.simVars->write(KEY_COM1_RECEIVE_SELECT, 1);
        globals.simVars->write(KEY_COM2_RECEIVE_SELECT, 1);
        receiveAllHideDelay = 60;
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
        switch (usingNav) {
        case 0:
            // Using NAV1
            activeFreq = simVars->nav1Freq;
            if (lastFreqAdjust == 0) {
                standbyFreq = simVars->nav1Standby;
            }
            if (loadedAircraft == AIRBUS_A310) {
                // A310 uses NAV1 for ILS frequency
                activeFreq = standbyFreq;
            }
            break;
        case 1:
            // Using NAV2
            activeFreq = simVars->nav2Freq;
            if (lastFreqAdjust == 0) {
                standbyFreq = simVars->nav2Standby;
            }
            break;
        case 2:
            // Using ADF
            activeFreq = simVars->adfFreq;
            if (lastFreqAdjust == 0) {
                if (loadedAircraft == CESSNA_152) {
                    // Cessna 152 doesn't have ADF standby
                    standbyFreq = activeFreq;
                }
                else {
                    standbyFreq = simVars->adfStandby;
                }
            }
            break;
        }
    }
    else if (simVars->com1Transmit == 1) {
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

    if (loadedAircraft == AIRBUS_A310) {
        // Current value is unknown so always read
        squawk = simVars->transponderCode;
    }
    else if (lastSquawkAdjust == 0) {
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
    int diff = (val - prevFreqWholeVal) / 2;
    bool switchBox = false;

    if (simVars->sbMode != 2) {
        prevFreqWholeValSb = simVars->sbEncoder[1];
    }
    else if (simVars->sbEncoder[1] != prevFreqWholeValSb) {
        val = simVars->sbEncoder[1];
        diff = val - prevFreqWholeValSb;
        if (diff == 0) {
            val = INT_MIN;
        }
        switchBox = true;
    }

    if (val != INT_MIN) {
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
                switch (usingNav) {
                case 0:
                {
                    double newVal = adjustNavWhole(adjust);
                    if (loadedAircraft == AIRBUS_A310) {
                        // A310 uses NAV1 to set ILS frequency
                        globals.simVars->write(KEY_NAV1_STBY_SET, standbyFreq);
                    }
                    else {
                        globals.simVars->write(KEY_NAV1_STBY_SET, newVal);
                    }
                    break;
                }
                case 1:
                {
                    double newVal = adjustNavWhole(adjust);
                    globals.simVars->write(KEY_NAV2_STBY_SET, newVal);
                    break;
                }
                case 2:
                {
                    double newVal = adjustAdf(standbyFreq, adjust, -1);
                    globals.simVars->write(KEY_ADF_STBY_SET, newVal);
                    if (loadedAircraft == CESSNA_152) {
                        // Cessna 152 doesn't have standby ADF so make it active immediately
                        globals.simVars->write(KEY_ADF1_RADIO_SWAP);
                    }
                    break;
                }
                }
            }
            else if (simVars->com1Transmit == 1) {
                // Using COM1
                double newVal = adjustComWhole(adjust);
                globals.simVars->write(KEY_COM1_STBY_RADIO_SET, newVal);
            }
            else {
                // Using COM2
                double newVal = adjustComWhole(adjust);
                globals.simVars->write(KEY_COM2_STBY_RADIO_SET, newVal);
            }
            if (switchBox) {
                prevFreqWholeValSb = val;
            }
            else {
                prevFreqWholeVal = val;
            }
        }
        fracSetSel = 0;
        time(&lastFreqAdjust);      // Gets reset by frac input
    }
}

void radio::gpioFreqFracInput()
{
    // Frequency fraction rotate
    int val = globals.gpioCtrl->readRotation(freqFracControl);
    int diff = (val - prevFreqFracVal) / 2;
    bool switchBox = false;

    if (simVars->sbMode != 2) {
        prevFreqFracValSb = simVars->sbEncoder[0];
    }
    else if (simVars->sbEncoder[0] != prevFreqFracValSb) {
        val = simVars->sbEncoder[0];
        diff = (val - prevFreqFracValSb);
        if (diff == 0) {
            val = INT_MIN;
        }
        switchBox = true;
    }

    if (val != INT_MIN) {
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
                switch (usingNav) {
                case 0:
                {
                    double newVal = adjustNavFrac(adjust);
                    if (loadedAircraft == AIRBUS_A310) {
                        // A310 uses NAV1 to set ILS frequency
                        globals.simVars->write(KEY_NAV1_STBY_SET, standbyFreq);
                    }
                    else {
                        globals.simVars->write(KEY_NAV1_STBY_SET, newVal);
                    }
                    break;
                }
                case 1:
                {
                    double newVal = adjustNavFrac(adjust);
                    globals.simVars->write(KEY_NAV2_STBY_SET, newVal);
                    break;
                }
                case 2:
                {
                    double newVal = adjustAdf(standbyFreq, adjust, fracSetSel);
                    globals.simVars->write(KEY_ADF_STBY_SET, newVal);
                    if (loadedAircraft == CESSNA_152) {
                        // Cessna 152 doesn't have standby ADF so make it active immediately
                        globals.simVars->write(KEY_ADF1_RADIO_SWAP);
                    }
                    break;
                }
                }
            }
            else if (simVars->com1Transmit == 1) {
                // Using COM1
                double newVal = adjustComFrac(adjust);
                globals.simVars->write(KEY_COM1_STBY_RADIO_SET, newVal);
            }
            else {
                // Using COM2
                double newVal = adjustComFrac(adjust);
                globals.simVars->write(KEY_COM2_STBY_RADIO_SET, newVal);
            }
            if (switchBox) {
                prevFreqFracValSb = val;
            }
            else {
                prevFreqFracVal = val;
            }
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
    int prevVal = prevFreqFracPush;
    switchBox = false;

    if (simVars->sbMode != 2 || prevFreqFracPushSb == 0) {
        prevFreqFracPushSb = simVars->sbButton[0];
    }
    else if (simVars->sbButton[0] != prevFreqFracPushSb) {
        val = simVars->sbButton[0];
        prevVal = prevFreqFracPushSb;
        switchBox = true;
    }

    if (val != INT_MIN) {
        if (prevVal % 2 == 1) {
            if (usingNav == 2) {
                // Short press switches ADF between 10s, units and half increments
                if (fracSetSel == 2) {
                    fracSetSel = 0;
                }
                else {
                    fracSetSel++;
                }
                time(&lastFreqAdjust);
                // Short press also switches ADF audio (morse code) on or off
                audioAdf = 1 - audioAdf;
                globals.simVars->write(KEY_RADIO_ADF_IDENT_SET, audioAdf);
            }
            else if (showNav) {
                // Short press switches NAV audio (morse code) on or off
                audioNav1 = 1 - audioNav1;
                if (usingNav == 0) {
                    globals.simVars->write(KEY_RADIO_VOR1_IDENT_SET, audioNav1);
                }
                else if (usingNav == 1) {
                    audioNav2 = 1 - audioNav2;
                    globals.simVars->write(KEY_RADIO_VOR2_IDENT_SET, audioNav2);
                }
            }
            else {
                // Short press switches COM between 10s and 100ths frequency increments
                if (fracSetSel == 1) {
                    fracSetSel = 0;
                }
                else {
                    fracSetSel = 1;
                }
            }
            receiveAllHideDelay = 60;
            time(&lastFreqPush);
        }
        if (val % 2 == 1) {
            // Released
            lastFreqPush = 0;
        }
        if (switchBox) {
            prevFreqFracPushSb = val;
        }
        else {
            prevFreqFracPush = val;
        }
        time(&lastFreqAdjust);
    }

    // Frequency fraction long push (over 1 sec)
    if (lastFreqPush > 0) {
        if (now - lastFreqPush > 1) {
            // Long press toggles receive ALL
            receiveAllHideDelay = 60;
            bool newVal = 1;
            if (simVars->com1Receive && simVars->com2Receive) {
                newVal = 0;
            }
            //if (simVars->com1Transmit == 1) {
            //    globals.simVars->write(KEY_COM1_RECEIVE_SELECT, 1);
            //    globals.simVars->write(KEY_COM2_RECEIVE_SELECT, newVal);
            //}
            //else {
            //    globals.simVars->write(KEY_COM2_RECEIVE_SELECT, 1);
            //    globals.simVars->write(KEY_COM1_RECEIVE_SELECT, newVal);
            //}
            lastFreqPush = 0;
        }
    }
}

void radio::gpioButtonsInput()
{
    // Swap push
    int val = globals.gpioCtrl->readPush(swapControl);
    int prevVal = prevSwapPush;
    int switchBox = false;

    if (simVars->sbMode != 2 || prevSwapPushSb == 0) {
        prevSwapPushSb = simVars->sbButton[6];
    }
    else if (simVars->sbButton[6] != prevSwapPushSb) {
        val = simVars->sbButton[6];
        prevVal = prevSwapPushSb;
        switchBox = true;
    }

    if (val != INT_MIN) {
        if (prevVal % 2 == 1) {
            // Swap active and standby frequencies
            if (showNav) {
                switch (usingNav) {
                case 0:
                    globals.simVars->write(KEY_NAV1_RADIO_SWAP);
                    break;
                case 1:
                    globals.simVars->write(KEY_NAV2_RADIO_SWAP);
                    break;
                case 2:
                    // Cessna 152 doesn't have standby ADF
                    if (loadedAircraft != CESSNA_152) {
                        globals.simVars->write(KEY_ADF1_RADIO_SWAP);
                    }
                    break;
                }
            }
            else if (simVars->com1Transmit == 1) {
                // Using COM1
                globals.simVars->write(KEY_COM1_RADIO_SWAP);
            }
            else {
                // Using COM2
                globals.simVars->write(KEY_COM2_RADIO_SWAP);
            }
            lastFreqAdjust = 0;
        }
        if (switchBox) {
            prevSwapPushSb = val;
        }
        else {
            prevSwapPush = val;
        }
    }

    // Com push
    val = globals.gpioCtrl->readPush(comControl);
    prevVal = prevComPush;
    switchBox = false;

    if (simVars->sbMode != 2 || prevComPushSb == 0) {
        prevComPushSb = simVars->sbButton[5];
    }
    else if (simVars->sbButton[5] != prevComPushSb) {
        val = simVars->sbButton[5];
        prevVal = prevComPushSb;
        switchBox = true;
    }

    if (val != INT_MIN) {
        if (prevVal % 2 == 1) {
            // If showing NAV, show COM1
            if (showNav) {
                showNav = false;
                globals.gpioCtrl->writeLed(comControl, !showNav);
                globals.gpioCtrl->writeLed(navControl, showNav);
            }
            else {
                // Already showing COM so switch between COM1 and COM2
                receiveAllHideDelay = 60;
                bool newVal = 0;
                if (simVars->com1Receive && simVars->com2Receive) {
                    newVal = 1;
                }
                if (simVars->com1Transmit == 1) {
                    // Switch transmit from COM1 to COM2
                    globals.simVars->write(KEY_COM2_TRANSMIT_SELECT);
                    globals.simVars->write(KEY_COM2_RECEIVE_SELECT, 1);
                    globals.simVars->write(KEY_COM1_RECEIVE_SELECT, newVal);
                }
                else {
                    // Switch transmit from COM2 to COM1
                    globals.simVars->write(KEY_COM1_TRANSMIT_SELECT);
                    globals.simVars->write(KEY_COM1_RECEIVE_SELECT, 1);
                    globals.simVars->write(KEY_COM2_RECEIVE_SELECT, newVal);
                }
            }
        }
        fracSetSel = 0;
        lastFreqAdjust = 0;
        if (switchBox) {
            prevComPushSb = val;
        }
        else {
            prevComPush = val;
        }
    }

    // Nav push
    val = globals.gpioCtrl->readPush(navControl);
    prevVal = prevNavPush;
    switchBox = false;

    if (simVars->sbMode != 2 || prevNavPushSb == 0) {
        prevNavPushSb = simVars->sbButton[4];
    }
    else if (simVars->sbButton[4] != prevNavPushSb) {
        val = simVars->sbButton[4];
        prevVal = prevNavPushSb;
        switchBox = true;
    }

    if (val != INT_MIN) {
        if (prevVal % 2 == 1) {
            // If showing COM, show NAV1
            if (!showNav) {
                showNav = true;
                globals.gpioCtrl->writeLed(comControl, !showNav);
                globals.gpioCtrl->writeLed(navControl, showNav);
            }
            else {
                // Already showing NAV so switch between NAV1, NAV2 and ADF
                if (loadedAircraft == AIRBUS_A310) {
                    usingNav = 0;
                }
                else {
                    if (usingNav == 2) {
                        usingNav = 0;
                    }
                    else {
                        usingNav++;
                    }
                }
            }

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
        if (switchBox) {
            prevNavPushSb = val;
        }
        else {
            prevNavPush = val;
        }
    }
}

void radio::gpioSquawkInput()
{
    // Squawk rotate
    int val = globals.gpioCtrl->readRotation(squawkControl);
    int diff = (val - prevSquawkVal) / 2;
    bool switchBox = false;

    if (simVars->sbMode != 2) {
        prevSquawkValSb = simVars->sbEncoder[3];
    }
    else if (simVars->sbEncoder[3] != prevSquawkValSb) {
        val = simVars->sbEncoder[3];
        diff = val - prevSquawkValSb;
        if (diff == 0) {
            val = INT_MIN;
        }
        switchBox = true;
    }

    if (val != INT_MIN) {
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
            if (loadedAircraft == AIRBUS_A310) {
                if (squawkSetSel == 0) {
                    globals.simVars->write(KEY_XPNDR_HIGH_SET, adjust);
                }
                else if (squawkSetSel == 2) {
                    globals.simVars->write(KEY_XPNDR_LOW_SET, adjust);
                }
            }
            else {
                globals.simVars->write(KEY_XPNDR_SET, newVal);
            }
            if (switchBox) {
                prevSquawkValSb = val;
            }
            else {
                prevSquawkVal = val;
            }
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
    int prevVal = prevSquawkPush;
    switchBox = false;

    if (simVars->sbMode != 2 || prevSquawkPushSb == 0) {
        prevSquawkPushSb = simVars->sbButton[3];
    }
    else if (simVars->sbButton[3] != prevSquawkPushSb) {
        val = simVars->sbButton[3];
        prevVal = prevSquawkPushSb;
        switchBox = true;
    }

    if (val != INT_MIN) {
        if (prevVal % 2 == 1) {
            // Short press switches to next digit
            if (loadedAircraft == AIRBUS_A310) {
                // Want sel to be 0 or 2
                squawkSetSel++;
            }
            if (lastSquawkAdjust == 0 || squawkSetSel == 3) {
                squawkSetSel = 0;
            }
            else {
                squawkSetSel++;
            }
            time(&lastSquawkPush);
            time(&lastSquawkAdjust);
        }
        if (val % 2 == 1) {
            // Released
            lastSquawkPush = 0;
        }
        if (switchBox) {
            prevSquawkPushSb = val;
        }
        else {
            prevSquawkPush = val;
        }
    }
    else if (lastSquawkAdjust != 0) {
        // Reset digit set selection if more than 2 seconds since last adjustment
        if (now - lastSquawkAdjust > 2) {
            squawkSetSel = 0;
            lastSquawkAdjust = 0;
        }
    }

    // Squawk long push (over 1 sec)
    if (lastSquawkPush > 0) {
        if (now - lastSquawkPush > 1) {
            if (loadedAircraft == AIRBUS_A310) {
                // Long press switches TCAS mode between Standby and TA/RA
                if (tcasMode == 0) {
                    tcasMode = 2;
                }
                else {
                    tcasMode = 0;
                }
                globals.simVars->write(KEY_XPNDR_STATE, tcasMode);
            }
            else if (loadedAircraft == FBW_A320) {
                // Long press switches transponder state between Standby and Auto
                transponderState = 1 - transponderState;
                globals.simVars->write(KEY_XPNDR_STATE, transponderState);
            }
            else if (!airliner) {
                // Long press switches transponder state between Off and Alt
                if (transponderState == 4) {
                    transponderState = 0;
                }
                else {
                    transponderState = 4;
                }
                globals.simVars->write(KEY_XPNDR_STATE, transponderState);
            }
            time(&lastTcasAdjust);
            lastSquawkPush = 0;
            lastSquawkAdjust = 0;
            squawkSetSel = 0;
        }
    }
    else if (lastTcasAdjust != 0) {
        if (now - lastTcasAdjust > 2) {
            lastTcasAdjust = 0;
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
            //globals.simVars->write(KEY_SPOILERS_ON);
            globals.simVars->write(KEY_SPOILERS_ARM_SET, 0);
            globals.simVars->write(KEY_SPOILERS_SET, 16384);
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
            // 0 = Auto, 1 = retracted, 2 = half, 3 = full
            switch (spoilersPos) {
            case 1:
                globals.simVars->write(KEY_SPOILERS_ARM_SET, 0);
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
        // Adjust 10ths, 100ths and 1000ths
        // 25 KHz spacing uses .000, .025, 0.050 and 0.075
        // 8.33 KHz spacing uses 0.005, 0.010, 0.015, 0.030, 0.035, 0.040, 0.055, 0.060, 0.065, 0.080, 0.085 and 0.090
        // So we can Skip .020, .045, .070 and .095
        frac2 += adjust * 5;
        if (adjust > 0) {
            if (frac2 > 90) {
                frac2 = 0;
                if (frac1 == 9) {
                    frac1 = 0;
                }
                else {
                    frac1++;
                }
            }
            switch (frac2) {
                case 20: frac2 = 25; break;
                case 45: frac2 = 50; break;
                case 70: frac2 = 75; break;
            }
        }
        else if (adjust < 0) {
            if (frac2 < 0) {
                frac2 = 90;
                if (frac1 == 0) {
                    frac1 = 9;
                }
                else {
                    frac1--;
                }
            }
            switch (frac2) {
                case 20: frac2 = 15; break;
                case 45: frac2 = 40; break;
                case 70: frac2 = 65; break;
                case 95: frac2 = 90; break;
            }
        }
    }
    else {
        // Adjust 10ths
        frac1 += adjust;

        if (frac1 > 9) {
            frac1 = 0;
        }
        else if (frac1 < 0) {
            frac1 = 9;
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

int radio::adjustAdf(double val, int adjust, int setSel)
{
    int whole = val;
    double rest = val - whole;
    int frac = (rest + 0.01) * 10;

    int highDigits = (whole % 10000) / 100;
    int digit3 = (whole % 100) / 10;
    int digit4 = whole % 10;
    int digit5 = frac;

    if (setSel == -1) {
        highDigits += adjust;
        if (highDigits >= 18) {
            highDigits -= 17;
        }
        else if (highDigits < 1) {
            highDigits += 17;
        }
    }
    else if (setSel == 0) {
        // Adjust 3rd digit
        digit3 = adjustDigit(digit3, adjust);
    }
    else if (setSel == 1) {
        // Adjust 4th digit
        digit4 = adjustDigit(digit4, adjust);
    }
    else {
        // Adjust 5th digit (must be 0 or 5)
        digit5 = adjustHalfDigit(digit5, adjust);
    }

    standbyFreq = highDigits * 100 + digit3 * 10 + digit4 + digit5 / 10.0;

    // Convert to BCD
    int digit1 = highDigits / 10;
    int digit2 = highDigits % 10;

    return 268435456.0 * digit1 + 16777216.0 * digit2 + 1048576.0 * digit3 + 65536 * digit4 + 4096 * digit5;
}

int radio::adjustDigit(int val, int adjust)
{
    val += adjust;
    if (val > 9) {
        val = 0;
    }
    else if (val < 0) {
        val = 9;
    }

    return val;
}

int radio::adjustHalfDigit(int val, int adjust)
{
    if (adjust != 0) {
        if (val >= 5) {
            val = 0;
        }
        else {
            val = 5;
        }
    }

    return val;
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
