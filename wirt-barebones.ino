/* wirt-barebones - Teensy USB to TRS MIDI Converter

  Adapted from https://github.com/PaulStoffregen/USBHost_t36/blob/master/examples/MIDI/Interface_16x16/Interface_16x16.ino

  This is the barebones version of my WIRT project - this requires no screen, buttons, encoders, etc, just MIDI in/out and USB
*/

#include <MIDI.h>         // access to serial (5 pin DIN) MIDI
#include <USBHost_t36.h>  // access to USB MIDI devices (plugged into 2nd USB port)

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

elapsedMillis displayMillis;      // how long the LED has been turned on
unsigned long displayTime = 500;  // how long something is displayed for

bool routing = true;  // enable USB to USB MIDI routing

void setup() {
  Serial.begin(9600);  // for debugging

  pinMode(13, OUTPUT);  // LED pin
  digitalWrite(13, LOW);

  MIDI.begin(MIDI_CHANNEL_OMNI);
  // Wait before turning on USB Host.  If connected USB devices
  // use too much power, Teensy at least completes USB enumeration, which
  // makes isolating the power issue easier.
  delay(750);

  myusb.begin();
  blinkLED(5);
}

void loop() {
  bool activity = false;

  myusb.Task();

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
  if (activity) {
    digitalWriteFast(13, HIGH);  // LED on
    displayMillis = 0;
  }
  if (displayMillis > 15) {
    digitalWriteFast(13, LOW);  // LED off
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
