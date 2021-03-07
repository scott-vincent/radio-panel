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
    int squawk;
    double setFreqFrac = 0;
    double setSquawk = 0;
    bool showNav = false;

    // Hardware controls
    int freqWholeControl = -1;
    int freqFracControl = -1;
    int swapControl = -1;
    int comControl = -1;
    int navControl = -1;
    int squawkControl = -1;

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

    time_t lastFreqAdjust = 0;
    time_t lastSquawkAdjust = 0;
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
    double adjustComWhole(int adjust);
    double adjustComFrac(int adjust);
    double adjustNavWhole(int adjust);
    double adjustNavFrac(int adjust);
    int adjustSquawk(int adjust);
    int adjustSquawkDigit(int val, int adjust);
};

#endif // _RADIO_H