#include "ScheduleParser.h"
#include <JsonListener.h>


void ScheduleListener::whitespace(char c) {
}

void ScheduleListener::startDocument() {
}

void ScheduleListener::key(String key) {
  currentKey = String(key);
}

void ScheduleListener::value(String value) {
  // Serial.printf("key: %s value:%s\n", currentKey.c_str(), value.c_str());
  if(currentKey == "alarmId") {
    alarmId = value.toInt();
  }
  if(currentKey == "zone") {
    zone = value.toInt();
  }
  if(currentKey == "dow") {
    dow = value.toInt();
  }
  if(currentKey == "duration") {
    duration = value.toInt();
  }
  if(currentKey == "hours") {
    hours = value.toInt();
  }
  if(currentKey == "mins") {
    mins = value.toInt();
  }
  if(currentKey == "secs") {
    secs = value.toInt();
  }
}

void ScheduleListener::endArray() {
}

void ScheduleListener::endObject() {
}

void ScheduleListener::endDocument() {
}

void ScheduleListener::startArray() {
}

void ScheduleListener::startObject() {
}

Schedule ScheduleListener::getSchedule() {
  Schedule s;
  s.zone = zone;
  s.dow = dow;
  s.alarmId = alarmId;
  s.duration = duration;
  s.hours = hours;
  s.mins = mins;
  s.secs = secs;
  return s;
}
