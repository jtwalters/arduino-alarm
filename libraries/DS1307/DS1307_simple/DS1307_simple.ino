/*
  DS1307: Real-Time Clock. Simple example
  Read more: www.jarzebski.pl/arduino/komponenty/zegar-czasu-rzeczywistego-rtc-ds1307.html
  GIT: https://github.com/jarzebski/Arduino-DS1307
  Web: http://www.jarzebski.pl
  (c) 2014 by Korneliusz Jarzebski
*/

#include <Wire.h>
#include <DS1307.h>

DS1307 clock;
RTCDateTime dt;

void setup()
{
  Serial.begin(9600);

  // Initialize DS1307
  Serial.println("Initialize DS1307");;
  clock.begin();
}

void loop()
{
  if (Serial.available()) {
    unsigned long t = processSyncMessage();
    if (t > 0) {
      clock.setDateTime(t);
    }
  }

  dt = clock.getDateTime();
  Serial.println(clock.dateFormat("F jS, Y - g:i:sa", dt));
  delay(1000);
}

/*  code to process time sync messages from the serial port   */
#define TIME_HEADER  "T"   // Header tag for serial time sync message

unsigned long processSyncMessage() {
  unsigned long pctime = 0L;
  const unsigned long DEFAULT_TIME = 1357041600; // Jan 1 2013 

  if(Serial.find(TIME_HEADER)) {
     pctime = Serial.parseInt();
     return pctime;
     if( pctime < DEFAULT_TIME) { // check the value is a valid time (greater than Jan 1 2013)
       pctime = 0L; // return 0 to indicate that the time is not valid
     }
  }
  return pctime;
}
