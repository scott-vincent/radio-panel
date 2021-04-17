echo Building radio-panel
cd radio-panel
g++ -lwiringPi -lpthread  \
    -o radio-panel \
    -I . \
    settings.cpp \
    simvarDefs.cpp \
    simvars.cpp \
    globals.cpp \
    gpioctrl.cpp \
    sevensegment.cpp \
    radio.cpp \
    radio-panel.cpp \
    || exit
echo Done
