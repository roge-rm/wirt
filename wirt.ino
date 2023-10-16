/* wirt - Teensy USB to TRS MIDI Converter

  Adapted from https://github.com/PaulStoffregen/USBHost_t36/blob/master/examples/MIDI/Interface_16x16/Interface_16x16.ino

  There's a lot going on here as I threw the hardware together before I really knew what I wanted to do with it.
  Basically this is a USB and TRS MIDI host that allows devices connected to either to talk to each other.
  I've also gone and added a screen and a couple of clickable encoders for functionality that is not yet realised
  and I've decided I'm going to use the SD slot for something as well (maybe saving device names, settings).

  Right now just the basic functionality is working, connecting devices via USB and sending signals sent via USB to
  other USB devices as well as the TRS MIDI out port. The TRS in port sends commands to the USB devices.

  Only the right encoder has function right now, it turns USB to USB routing on/off. 
  At some point I will set up selectable device routing (maybe with saving to SD!).
*/

#include <MIDI.h>         // access to serial (5 pin DIN) MIDI
#include <USBHost_t36.h>  // access to USB MIDI devices (plugged into 2nd USB port)
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>  // display
#include <EEPROM.h>
#include <Wire.h>
#include <EncButton.h>

/*#include <SD.h>  // SD access for saving/restoring data

const int chipSelect = BUILTIN_SDCARD;
File myFile; */

#define enc1PinA 24
#define enc1PinB 25
#define enc1PinBut 30
#define enc2PinA 26
#define enc2PinB 27
#define enc2PinBut 31

#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels

// 128x64 OLED SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire1, -1);

// create the Serial MIDI port
MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI);

// create the ports for USB devices plugged into Teensy 4.1 USB Host Port (wired to 4 port hub, but allow for additonal hubs and devices)
USBHost myusb;
USBHub hub1(myusb);
USBHub hub2(myusb);
USBHub hub3(myusb);
USBHub hub4(myusb);
MIDIDevice midi01(myusb);
MIDIDevice midi02(myusb);
MIDIDevice midi03(myusb);
MIDIDevice midi04(myusb);
MIDIDevice midi05(myusb);
MIDIDevice midi06(myusb);
MIDIDevice midi07(myusb);
MIDIDevice midi08(myusb);
MIDIDevice midi09(myusb);
MIDIDevice midi10(myusb);
MIDIDevice *midilist[10] = {
  &midi01, &midi02, &midi03, &midi04, &midi05, &midi06, &midi07, &midi08, &midi09, &midi10
};

elapsedMillis displayMillis;      // ow long the LED has been turned on
unsigned long displayTime = 500;  // how long something is displayed for
unsigned long versionNUM = 20231016;

bool routing = true;  // enable USB to USB MIDI routing
bool displayUpdate;   // used to reduce number of screen updates

bool activityLED = true;  // blink LED when activity is detected

// encoder/button setup
EncButton enc1(enc1PinA, enc1PinB, enc1PinBut, INPUT_PULLUP);
EncButton enc2(enc2PinA, enc2PinB, enc2PinBut, INPUT_PULLUP);

void setup() {
  Serial.begin(9600);  // for debugging

  pinMode(13, OUTPUT);  // LED pin
  digitalWrite(13, LOW);

  // screen initialization
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {  // address 0x3C for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;  // Don't proceed, loop forever
  }

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.display();

  display.setRotation(2);  // set display upside down to match hardware (my screen is oriented upside down for "reasons")
  display.clearDisplay();

  MIDI.begin(MIDI_CHANNEL_OMNI);
  // Wait before turning on USB Host.  If connected USB devices
  // use too much power, Teensy at least completes USB enumeration, which
  // makes isolating the power issue easier.
  delay(750);

  display.clearDisplay();
  Serial.println("wirt - Teensy USB to TRS MIDI Converter");

  displayCentre("wirt", 15, 2);
  displayCentre("MIDI router", 35, 1);
  displayCentre(versionNUM, 55, 1);
  delay(1250);

  /* SD stuff, not fleshed out yet
  Serial.print("Initializing SD card...");

  if (!SD.begin(chipSelect)) {
    Serial.println("initialization failed!");
    displayCentre("SD Init Failed", 30, 1);
    return;
  }
  Serial.println("initialization done.");
  display.clearDisplay();
  displayCentre("SD Init Successful", 30, 1);
  delay(750);

  myFile = SD.open("test.cfg", FILE_WRITE);  // open settings file

  if (myFile) {
    Serial.print("Writing to test.cfg...");
    myFile.println("testing 1, 2, 3.");
    // close the file:
    myFile.close();
    Serial.println("done.");
  } else {
    // if the file didn't open, print an error:
    Serial.println("error opening test.cfg");
  }
*/

  myusb.begin();
  blinkLED(5);

  display.clearDisplay();
  display.display();
}

void loop() {
  bool activity = false;

  myusb.Task();

  encUpdate();  // update encoders
  encAction();  // do actions based on encoder input

  // first read messages from the TRS MIDI IN port
  if (MIDI.read()) {
    sendToComputer(MIDI.getType(), MIDI.getData1(), MIDI.getData2(), MIDI.getChannel(), MIDI.getSysExArray(), 0);
    activity = true;
  }

  // next read messages arriving from the devices plugged into the USB hub
  for (int port = 0; port < 10; port++) {
    if (midilist[port]->read()) {
      uint8_t type = midilist[port]->getType();
      uint8_t data1 = midilist[port]->getData1();
      uint8_t data2 = midilist[port]->getData2();
      uint8_t channel = midilist[port]->getChannel();
      const uint8_t *sys = midilist[port]->getSysExArray();
      sendToComputer(type, data1, data2, channel, sys, 0);

      if (routing) {  // send data from one USB device to another to act as a USB router
        for (int port2 = 0; port2 < 10; port2++) {
          if (port2 != port) midilist[port2]->send(type, data1, data2, channel);
        }
      }

      midi::MidiType mtype = (midi::MidiType)type;
      MIDI.send(mtype, data1, data2, channel);
      activity = true;
    }
  }

  // read messages from PC/host device
  if (usbMIDI.read()) {
    // get the USB MIDI message, defined by these 5 numbers (except SysEX)
    byte type = usbMIDI.getType();
    byte channel = usbMIDI.getChannel();
    byte data1 = usbMIDI.getData1();
    byte data2 = usbMIDI.getData2();

    // forward this message to 1 of the 3 Serial MIDI OUT ports
    if (type != usbMIDI.SystemExclusive) {
      // Normal messages, first we must convert usbMIDI's type (an ordinary
      // byte) to the MIDI library's special MidiType.
      midi::MidiType mtype = (midi::MidiType)type;
      MIDI.send(mtype, data1, data2, channel);
      for (int port = 0; port < 10; port++) {
        midilist[port]->send(type, data1, data2, channel);
      }

    } else {
      // SysEx messages are special.  The message length is given in data1 & data2
      unsigned int SysExLength = data1 + data2 * 256;
      MIDI.sendSysEx(SysExLength, usbMIDI.getSysExArray(), true);
    }
    activity = true;
  }

  // blink the LED when any activity has happened
  if (activityLED) { // but only if the activity LED is enabled
    if (activity) {
      digitalWriteFast(13, HIGH);  // LED on
      displayMillis = 0;
    }
    if (displayMillis > 15) {
      digitalWriteFast(13, LOW);  // LED off
    }
  }

  if (displayUpdate) {
    if (displayMillis > displayTime) {
      display.clearDisplay();
      display.display();
      displayUpdate = false;
    }
  }
}

void blinkLED(int blinks) {
  for (int i = 0; i < blinks; i++) {
    digitalWriteFast(13, HIGH);
    delay(50);
    digitalWriteFast(13, LOW);
    delay(50);
  }
}

void sendToComputer(byte type, byte data1, byte data2, byte channel, const uint8_t *sysexarray, byte cable) {
  if (type != midi::SystemExclusive) {
    usbMIDI.send(type, data1, data2, channel, cable);
  } else {
    unsigned int SysExLength = data1 + data2 * 256;
    usbMIDI.sendSysEx(SysExLength, sysexarray, true, cable);
  }
}

void encUpdate() {  // poll buttons and encoders for updates
  enc1.tick();
  enc2.tick();
}

void encAction() {  // do things when encoders are modified
  if (enc1.click()) {
    display.clearDisplay();
    displayCentre("enc1 click", 20, 1);
    displayUpdate = true;
    displayMillis = 0;
    displayTime = 1000;
  } else if (enc1.left()) {
    display.clearDisplay();
    displayCentre("enc1 left", 20, 1);
    displayUpdate = true;
    displayMillis = 0;
    displayTime = 1000;
  } else if (enc1.right()) {
    display.clearDisplay();
    displayCentre("enc1 right", 20, 1);
    displayUpdate = true;
    displayMillis = 0;
    displayTime = 1000;
  } else if (enc2.click()) {  // show USB routing status
    display.clearDisplay();
    displayCentre("routing", 20, 1);
    if (routing) displayCentre("enabled", 35, 1);
    else displayCentre("disabled", 35, 1);
    displayUpdate = true;
    displayMillis = 0;
    displayTime = 1500;
  } else if (enc2.left()) {  // disable USB routing
    display.clearDisplay();
    routing = false;
    displayCentre("routing", 20, 1);
    displayCentre("disabled", 35, 1);
    displayUpdate = true;
    displayMillis = 0;
    displayTime = 1500;
  } else if (enc2.right()) {  // enable USB routing
    display.clearDisplay();
    routing = true;
    displayCentre("routing", 20, 1);
    displayCentre("enabled", 35, 1);
    displayUpdate = true;
    displayMillis = 0;
    displayTime = 1500;
  }
}

void displayCentre(String text, int y, int size) {  // display text in centre of screen - inputs are text to output and y position of text
  int16_t x1, y1;
  uint16_t w, h;
  display.setTextSize(size);
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);  // calculate width of text
  display.setCursor((128 - w) / 2, y);                  // centre text
  display.print(text);
  display.display();
}

void displayText(String text, int x, int y, int size) {
  display.setTextSize(size);
  display.setCursor(x, y);
  display.print(text);
  display.display();
}
