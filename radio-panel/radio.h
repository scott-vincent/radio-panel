#ifndef _RADIO_H_
#define _RADIO_H_

#include "simvars.h"
#include "sevensegment.h"

class radio
{
private:
    SimVars* simVars;
    Aircraft loadedAircraft = UNDEFINED;
    sevensegment* sevenSegment;

    unsigned char display1[8];
    unsigned char display2[8];
    unsigned char display3[8];
    double activeFreq;
    double standbyFreq;
    int transponderState = -1;
    int squawk;
    double setFreqFrac = 0;
    double setSquawk = 0;
    bool showNav = false;
    int spoilersVal = INT_MIN;
    int spoilersAutoVal = -6;       // Default to spoilers = retracted
    int spoilersDownVal = 14;
    int lastSpoilersPos = -1;       // 0 = auto, 1 = retracted, 2 = half, 3 = full
    bool gearDown = true;
    bool showSeatBelts = false;

    // Hardware controls
    int freqWholeControl = -1;
    int freqFracControl = -1;
    int swapControl = -1;
    int comControl = -1;
    int navControl = -1;
    int squawkControl = -1;
    int trimWheelControl = -1;
    int spoilersAutoControl = -1;
    int spoilersPosControl = -1;
    int spoilersDownControl = -1;
    int gearUpControl = -1;
    int gearDownControl = -1;
    int seatBeltsControl = -1;

    int prevFreqWholeVal = 0;
    int prevFreqFracVal = 0;
    int prevFreqFracPush = 0;
    int fracSetSel = 0;
    int prevSwapPush = 0;
    int prevComPush = 0;
    int prevNavPush = 0;
    int prevSquawkVal = 0;
    int prevSquawkPush = 0;
    int squawkSetSel = 0;
    int prevTrimWheelVal = 0;
    int prevSpoilersAutoToggle = -1;
    int prevSpoilersDownToggle = -1;
    int prevGearUpToggle = -1;
    int prevGearDownToggle = -1;

    time_t lastFreqAdjust = 0;
    time_t lastSquawkAdjust = 0;
    time_t lastSquawkPush = 0;
    time_t now;

public:
    radio();
    void render();
    void update();

private:
    void blankDisplays();
    void writeCom(unsigned char* display, double freq);
    void writeNav(unsigned char* display, double freq);
    void addGpio();
    void gpioFreqWholeInput();
    void gpioFreqFracInput();
    void gpioButtonsInput();
    void gpioSquawkInput();
    void gpioTrimWheelInput();
    void gpioSpoilersInput();
    void gpioGearInput();
    double adjustComWhole(int adjust);
    double adjustComFrac(int adjust);
    double adjustNavWhole(int adjust);
    double adjustNavFrac(int adjust);
    int adjustSquawk(int adjust);
    int adjustSquawkDigit(int val, int adjust);
};

#endif // _RADIO_H
