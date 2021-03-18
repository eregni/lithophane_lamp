// RGB LAMP with fading colors
/*
More ideas:
    CODE:
    - white color option
    - remove bootloader
    - eeprom wear leveling
    - maybe: low(er) power library?
    HARDWARE:
    - adjust size of the litho cube (to the 2cv)
    - move the usb input plug away from the side
    - something to hold the power cable properly (is 
     now glued)
    - Funny -> There is an optical illusion effect possible when completly fading on/off the led (I was unable to recreate the effect)
    - Lower power consumtion
/*

/* (reminder) use <> to make the compiler look in the default library directory 
and "" to first look in the working directory */
#include <SPI.h>                       
#include <EEPROM.h>
#include "gamma_correction.h"  // Lookup tables for gamma correction
///////////////
// DEBUGGING //
///////////////
// uncomment to activate debugging. 'DEBUG' is required, the rest is optional
//#define DEBUG               // Some general stuff
//#define DEBUG_ANIMATION     // led animation data (Produces a lot of info)
//#define DEBUG_BUTTON        // button activities

////////////
// CONFIG //
////////////
#ifdef DEBUG
    const String programVersion = F"1.0";
    const String programName = F"Lithophane lamp with rgb led animation";
#endif
byte nrOfLeds = 16;
SPISettings spiSettings(10000000, MSBFIRST, SPI_MODE0);
const boolean GAMMACORRECTION = true;
const byte STEP = 1;     // RGB value change per tick during the animation
const byte address = 0;  // EEPROM start address

// The colors for fading animation
/// HSV Rainbow color palette  from fastled library
const long PALETTE_HEX[17] = {
    0xFFFFFF,
    0xFF0000, 0xD52A00, 0xAB5500, 0xAB7F00,
    0xABAB00, 0x56D500, 0x00FF00, 0x00D52A,
    0x00AB55, 0x0056AA, 0x0000FF, 0x2A00D5,
    0x5500AB, 0x7F0081, 0xAB0055, 0xD5002B
};

// Button panel
const byte BUTTONS[3] = {A0, A1, A2};
const byte BUTTON1_PIN = A0;
const byte BUTTON2_PIN = A1;
const byte BUTTON3_PIN = A2;
const int DEBOUNCE = 100;  // Millis
////////////////////////////////////////////////////////////////////////
// END CONFIG //////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////

///////////////
// VARIABLES //
///////////////
unsigned long timestamp;

// Button panel
unsigned long buttonTimestamp = 0;
bool buttonPressed = false;

// Color type
typedef struct{
    byte red;
    byte green;
    byte blue;
} color;

// Settings for the user with default values
typedef struct{
    byte brightness = 0b00011111;
    byte speed = 0; // This value points to the color in the list_speed array
    byte color = 0; // This value points to the color in the palette array
} settings;
settings userSettings;

enum option{
    BRIGHTNESS,
    SPEED,
    COLOR,
    CHECK
};

// Speed
const unsigned int list_speed[5]= {0, 5000, 500, 50, 5};

// Colors
bool timer = false; // timer for the fading color animation
const color OFF = {0, 0, 0};
const color WHITE = {255, 255, 255};
color palette[sizeof(PALETTE_HEX) / 4];
byte nrOfColors;
color currentColor; // This variable is used by the animation function
color targetColor;

// Direction the rgb value is moving: 0 = hold. 1 = increase. -1 = decrease
signed int redDirection = 0;
signed int greenDirection = 0;
signed int blueDirection = 0;
////////////////////////////////////////////////////////////////////////
// END VARIABLES ///////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////

///////////////
// FUNCTIONS //
///////////////
void initialize(){
    
    // Load table with all rgb values in memory
    for (int i = 0; i < sizeof(PALETTE_HEX) / sizeof(PALETTE_HEX[0]); i++){
        palette[i] = hexToColor(PALETTE_HEX[i]);
    }
    nrOfColors = sizeof(palette) / sizeof(palette[0]);
    // Get settings from Eeprom
    byte brightnessIn = EEPROM.read(BRIGHTNESS);
    byte speedIn = EEPROM.read(SPEED);
    byte colorIn = EEPROM.read(COLOR);
    byte checkNr = 128 - brightnessIn - speedIn - colorIn;

    #ifdef DEBUG
        Serial.println(EEPROM.read(BRIGHTNESS));
        Serial.println(EEPROM.read(SPEED));
        Serial.println(EEPROM.read(COLOR));
        Serial.print(F("Check byte and check nr: "));
        Serial.println(EEPROM.read(CHECK));
        Serial.println(checkNr);
    #endif
     
    if (EEPROM.read(CHECK) != checkNr ){
        #ifdef DEBUG
            Serial.println(F"incorrect CHECK byte. Using default setting"));
        #endif
        // If the CHECK byte is incorrect we will use default settings
        EEPROM.update(BRIGHTNESS, userSettings.brightness);
        EEPROM.update(SPEED, userSettings.speed);
        EEPROM.update(COLOR, userSettings.color);
        // A little bit of integrity check
        // The CHECK byte is 128 - sum of all settings. The sum will never be greater than 31 + 5 + 15
        EEPROM.update(CHECK, 128 - userSettings.brightness - userSettings.speed - userSettings.color);
    }
    else{
        #ifdef DEBUG
            Serial.println(F"Checkbyte OK"));
        #endif
        userSettings.brightness = brightnessIn;                     
        userSettings.speed = speedIn;  
        userSettings.color = colorIn;  
    }

    currentColor = targetColor = palette[userSettings.color];
}

void writeSetting(option option, byte value){
    #ifdef DEBUG
        Serial.println(EEPROM.read(BRIGHTNESS));
        Serial.println(EEPROM.read(SPEED));
        Serial.println(EEPROM.read(COLOR));
        Serial.print(F("Check byte and check nr: "));
        Serial.println(EEPROM.read(CHECK));
    #endif
    EEPROM.update(option, value);
    EEPROM.update(CHECK, 128 - userSettings.brightness - userSettings.speed - userSettings.color);
    wait(10);
}

// Color Animation. The function needs to be called for every step
void playColors(){
    #ifdef DEBUG_ANIMATION
        Serial.println(F("\n --- Playcolors ---"));
        Serial.println(F("Current color:"));
        Serial.print(F("Red: "));
        Serial.println(currentColor.red);
        Serial.print(F("Green: "));
        Serial.println(currentColor.green);
        Serial.print(F("Blue: "));
        Serial.println(currentColor.blue);
        Serial.print(F("Compare colors: "));
        Serial.println(compareColor(currentColor, targetColor));
        Serial.print(F("Color pointer"));
        Serial.println(userSettings.color);
        Serial.println(F("-----------"));
    #endif
    if (compareColor(palette[userSettings.color], WHITE)){  // Skip the white color in the animation
        userSettings.color++;
        writeSetting(COLOR, userSettings.color);
        targetColor, currentColor = palette[userSettings.color]; 
    }
    
    compareColor(currentColor, targetColor) ? adjustRGBsteps() : writeColor(currentColor, userSettings.brightness);

    currentColor.red += STEP * redDirection;
    currentColor.green += STEP * greenDirection;
    currentColor.blue += STEP * blueDirection; 

    //Stop changing a color when it reached his target value
    if (currentColor.red == targetColor.red){
        redDirection = 0;
    }
    if (currentColor.green == targetColor.green){
        greenDirection = 0;
    }
    if (currentColor.blue == targetColor.blue){
        blueDirection = 0;
    }
}

void fadein(color fadeColor){
    for (int i = 0; i <= userSettings.brightness; i++){
        writeColor(fadeColor, i);
        wait(20);
    }
}

void fadeOut(){
    for (int i = userSettings.brightness; i > 0; i--){
        writeColor(palette[userSettings.color], i);
        wait(10);
    }
}

// Change the led color to the current RGB settings
void writeColor(color newColor, byte _brightness){
    if (GAMMACORRECTION){
        newColor.red =  pgm_read_byte(&gamma_correction_LUT::RED[newColor.red]);
        newColor.green = pgm_read_byte(&gamma_correction_LUT::GREEN[newColor.green]);
        newColor.blue = pgm_read_byte(&gamma_correction_LUT::BLUE[newColor.blue]);
    }

    #ifdef DEBUG_ANIMATION
        Serial.print(F("WRITE NEW VALUES (+ gammacorrection):\nRed:\t\t"));
        Serial.println(newColor.red);
        Serial.print(F("Green:\t\t"));
        Serial.println(newColor.green);
        Serial.print(F("Blue:\t\t"));
        Serial.println(newColor.blue);
        Serial.print(F("Brightness:\t"));
        Serial.println(_brightness, BIN);
    #endif
    
    _brightness |= 0b11100000;  // The brightness value starts from 0b11100000 according to datasheet

    //Start frame
    SPI.transfer(0000000000);
    SPI.transfer(0b00000000);
    SPI.transfer(0b00000000);
    SPI.transfer(0b00000000);

    for (int i = 0; i < nrOfLeds; i++){
        // LED --> 111 + 5bit(brightness) + 8bit + 8bit + 8bit (BGR)
        SPI.transfer(_brightness);
        SPI.transfer(newColor.blue);
        SPI.transfer(newColor.green);
        SPI.transfer(newColor.red);
    }

    //End frame
    SPI.transfer(0b11111111);
    SPI.transfer(0b11111111);
    SPI.transfer(0b11111111);
    SPI.transfer(0b11111111);

    #ifdef DEBUG_ANIMATION
        Serial.println(F("SPI TRANSFER OK"));
        Serial.println(F("-----------"));
    #endif
}

// Set the next color the led will change to
void adjustRGBsteps(){ 
    userSettings.color == nrOfColors - 1 ? userSettings.color = 1 : userSettings.color++;  // Skip white color
    color nextTarget = palette[userSettings.color];

    // Change RGB direction accordingly to 0 | 1 | -1
    redDirection =  nextTarget.red - currentColor.red;
    if (redDirection != 0){
        redDirection = (nextTarget.red - currentColor.red) / (abs(nextTarget.red - currentColor.red));
    }

    greenDirection = nextTarget.green - currentColor.green;
    if (greenDirection != 0){
        greenDirection = (nextTarget.green - currentColor.green) / (abs(nextTarget.green - currentColor.green));
    }

    blueDirection = nextTarget.blue - currentColor.blue;
    if (blueDirection != 0){
        blueDirection = (nextTarget.blue - currentColor.blue) / (abs(nextTarget.blue - currentColor.blue));
    }

    targetColor = nextTarget;

    #ifdef DEBUG_ANIMATION
        Serial.println(F("NEW RGB DIRECTIONS:"));
        Serial.print(F("color pointer: "));
        Serial.print(userSettings.color);
        Serial.print(F("/"));
        Serial.println(nrOfColors);
        Serial.print(F("Red direction: "));
        Serial.println(redDirection);
        Serial.print(F("green direction: "));
        Serial.println(greenDirection);
        Serial.print(F("blue direction: "));
        Serial.println(blueDirection);
        Serial.print(F("Red next value = "));
        Serial.println(targetColor.red);
        Serial.print(F("green next value = "));
        Serial.println(targetColor.green);
        Serial.print(F("blue next value = "));
        Serial.println(targetColor.blue);
        Serial.println(F("-----------"));
    #endif
}

color hexToColor(long hexColor){
    //From left to right every two hex digits represents a color value
    byte red = (hexColor & 0xFF0000) >> 16;
    byte green = (hexColor & 0xFF00) >> 8;
    byte blue = hexColor & 0xFF;
    return color {red, green, blue};
}
    
bool compareColor(color color0, color color1){
    if (color0.red != color1.red){
        return false;
    }
    if (color0.green != color1.green){
        return false;
    }
    if (color0.blue != color1.blue){
        return false;
    }
    return true;
}

void adjustSpeed(){
    if (userSettings.speed >= (sizeof(list_speed) / 2) - 1){
        userSettings.speed = 0;
        Serial.println(userSettings.speed);
        writeSetting(SPEED, userSettings.speed);
        writeColor(currentColor, userSettings.brightness >> 2);
        wait(200);
        writeColor(currentColor, userSettings.brightness);
        wait(200);
        writeColor(currentColor, userSettings.brightness >> 2);
        wait(200);
        writeColor(currentColor, userSettings.brightness);
        #ifdef DEBUG_ANIMATION
            Serial.println(F("*** Animation OFF ***"));
        #endif
    }
    else{
        userSettings.speed++;
        writeSetting(SPEED, userSettings.speed);
        writeColor(currentColor, userSettings.brightness >> 1);
        wait(DEBOUNCE + ((sizeof(list_speed) / 2) - userSettings.speed) * DEBOUNCE);
        writeColor(currentColor, userSettings.brightness);
    }
    #ifdef DEBUG_BUTTON
        Serial.print(F("*** New speed: "));
        Serial.print(userSettings.speed);
        Serial.println(F(" ***"));
    #endif
} 

void adjustBrightness(){
    if (userSettings.brightness >= 0b00011111){
        userSettings.brightness = 0b000000001;
        writeSetting(BRIGHTNESS, userSettings.brightness);
    }
    else{
        userSettings.brightness = (userSettings.brightness << 1) | 0b00000001;
        writeSetting(BRIGHTNESS, userSettings.brightness);
    }
    #ifdef DEBUG_BUTTON
        Serial.print(F("*** New brightness: "));
        Serial.print(userSettings.brightness);
        Serial.println(F(" ***"));
    #endif

    // Refresh the led if no animtion is playing
    if (userSettings.speed == 0){
        writeColor(palette[userSettings.color], userSettings.brightness);
    }
    else {
        writeColor(currentColor, userSettings.brightness);
    }
}

void adjustColor(){
    userSettings.speed = 0;
    fadeOut();
    userSettings.color >= nrOfColors - 1 ? userSettings.color = 0 : userSettings.color++;
    #ifdef DEBUG_BUTTON
        Serial.print(F("*** New color: "));
        Serial.print(userSettings.color);
        Serial.println(F(" ***"));
    #endif
    currentColor = palette[userSettings.color];
    writeSetting(COLOR, userSettings.color);
    fadein(palette[userSettings.color]);
}

void wait(long idleTime){
    long timestamp = millis();
    while (millis() - timestamp < idleTime);
}
////////////////////////////////////////////////////////////////////////
// END FUNCTIONS ///////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////

//////////////
// PROGRAM  //
//////////////
void setup(){
    #ifdef DEBUG
        Serial.begin(115200);
    #endif
    pinMode(BUTTON1_PIN, INPUT_PULLUP);
    pinMode(BUTTON2_PIN, INPUT_PULLUP);
    pinMode(BUTTON3_PIN, INPUT_PULLUP);
    SPI.begin();
    SPI.beginTransaction(spiSettings);
    initialize();
    #ifdef DEBUG
        Serial.println(F("\n\n --- START ---\n---------------"));
        Serial.println(programName);
        Serial.print(F("Version"));
        Serial.println(programVersion);
        Serial.println(F"SETTINGS:"));
        Serial.print(F("Brightness: "));
        Serial.println(userSettings.brightness);
        Serial.print(F("Speed: "));
        Serial.println(userSettings.speed);
        Serial.print(F"Color: "));
        Serial.println(userSettings.color);
    #endif
    fadein(palette[userSettings.color]);
}

void loop(){
    // Check if the animation is playing
    if (list_speed[userSettings.speed] != 0 && timer){
        playColors();
        timestamp = millis();
        timer = false;
    }   
    else if (millis() - timestamp > list_speed[userSettings.speed]){
        timer = true;
    } 

    // Check buttons
    if (millis() - buttonTimestamp > DEBOUNCE){
        byte buttonInput = 0;
        // Read all buttons
        for (int i = 0; i < 3; i++){
            buttonInput = !digitalRead(BUTTONS[i]);
            if (buttonInput > 0){
                buttonInput = i + 1;
                buttonTimestamp = millis();
                break;
            }
        }
        // Check if button was released after last button press 
        if (buttonPressed && buttonInput == 0){
            buttonPressed = false;
        } 

        // Valid button press
        if (!buttonPressed && buttonInput > 0){
            buttonPressed = true;
            #ifdef DEBUG_BUTTON
                Serial.print(F("PING BUTTON "));
                Serial.println(buttonInput);
            #endif
            switch(buttonInput){
                case 1:
                    adjustBrightness();
                    break;
                case 2:
                    adjustSpeed();
                    break;
                case 3:
                    adjustColor();
                    break;
                default:
                    break;
            }                
        }
    }
}
////////////////////////////////////////////////////////////////////////
// END PROGRAM /////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////