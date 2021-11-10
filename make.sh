echo Building radio-panel
cd radio-panel
g++ -o radio-panel -I . \
    settings.cpp \
    simvarDefs.cpp \
    simvars.cpp \
    globals.cpp \
    gpioctrl.cpp \
    sevensegment.cpp \
    radio.cpp \
    radio-panel.cpp \
    -lwiringPi -lpthread || exit
echo Done
