#include <Arduino.h>
#include <Ticker.h>
#include <Time.h>
#include <TimeAlarms.h>
#include <NtpClientLib.h>
#include <ScheduleParser.h>
#include <JsonStreamingParser.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <SSD1306Brzo.h>
#include <DS1307RTC.h>
#include <Encoder.h>
#include <Wire.h>
#include <OLEDDisplayUi.h>
#include <symbols.h>

const char WIFI_SSID[] = "irrigationWiFi";
const char WIFI_PASSWD[] = "passw0rd";
const char HOSTNAME[] = "irrigation";
const char NTP_SERVER[] = "pool.ntp.org";
const int TIME_ZONE = -8; // PST
const bool DAYLIGHT = true;
const int UPDATE_INTERVAL_SECS = 10 * 60; // Update every 10 minutes
const float UTC_OFFSET = -7;
const boolean IS_METRIC = true;
const unsigned long WATCHDOG_TOO_LONG = 1000L * 60L * 2L; // 2 mins in milliseconds
const timeDayOfWeek_t DOW[7] = {dowSunday, dowMonday, dowTuesday, dowWednesday, dowThursday, dowFriday, dowSaturday}; // dow[0] = Sunday
const unsigned int SCHEDULE_MAX = 8;

bool readyForUpdate = false;
volatile long encPosition  = -999;
volatile long lastPosition  = -999;
volatile unsigned int scheduleIdx = 0;

ESP8266WebServer server(80);
SSD1306Brzo display(0x3c, D2, D1);
OLEDDisplayUi ui( &display );
Ticker ticker;
Schedule schedules[8];
Encoder enc(D5, D6);

void drawProgress(OLEDDisplay *display, int percentage, String label);
void updateData(OLEDDisplay *display);
void drawZone1(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawZone2(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawZone3(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawZone4(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawZone5(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawZone6(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawZone7(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawZone8(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState* state);
void setReadyForUpdate();
int8_t getWifiQuality();
FrameCallback frames[] = { drawZone1, drawZone2, drawZone3, drawZone4, drawZone5, drawZone6, drawZone7, drawZone8 };
int numberOfFrames = 8;

OverlayCallback overlays[] = { drawHeaderOverlay };
int numberOfOverlays = 1;
void setup() {
        Serial.begin(74880);
        Serial.println("===========================================================");

        rtcSync();
        logTime();

        display.init();
        display.clear();
        display.display();
        display.setFont(ArialMT_Plain_10);
        display.setTextAlignment(TEXT_ALIGN_CENTER);
        display.setContrast(255);

        WiFi.hostname(HOSTNAME);
        WiFi.mode(WIFI_STA);
        WiFi.begin(WIFI_SSID, WIFI_PASSWD);

        int counter = 0;

        while (WiFi.status() != WL_CONNECTED) {
                delay(500);

                Serial.print(".");
                display.clear();
                display.flipScreenVertically();
                display.drawString(64, 10, "Connecting to WiFi");
                display.drawXbm(46, 30, 8, 8, counter % 3 == 0 ? activeSymbol : inactiveSymbol);
                display.drawXbm(60, 30, 8, 8, counter % 3 == 1 ? activeSymbol : inactiveSymbol);
                display.drawXbm(74, 30, 8, 8, counter % 3 == 2 ? activeSymbol : inactiveSymbol);
                display.display();

                counter++;
        }

        MDNS.begin(HOSTNAME);
        IPAddress local_ip = WiFi.localIP();
        Serial.printf("\nIP address: %s\n", local_ip.toString().c_str());

        NTP.begin(NTP_SERVER, TIME_ZONE, DAYLIGHT);
        NTP.setInterval(1, DEFAULT_NTP_INTERVAL);

        server.on("/", HTTP_GET, handleRoot);
        server.on("/scheduler", HTTP_POST, handleRequest);
        server.on("/scheduler", HTTP_DELETE, handleDelete);
        server.begin();

        if (MDNS.begin(HOSTNAME)) {
                Serial.println("MDNS responder started");
                MDNS.addService("http", "tcp", 80);
        }

        ui.setTargetFPS(30);

        //Hack until disableIndicator works:
        //Set an empty symbol
        ui.setActiveSymbol(emptySymbol);
        ui.setInactiveSymbol(emptySymbol);
        ui.disableIndicator();

        // You can change the transition that is used
        // SLIDE_LEFT, SLIDE_RIGHT, SLIDE_TOP, SLIDE_DOWN
        ui.setFrameAnimation(SLIDE_LEFT);
        ui.setFrames(frames, numberOfFrames);
        ui.setOverlays(overlays, numberOfOverlays);
        ui.init();
        display.flipScreenVertically();

        ui.disableAutoTransition();

        updateData(&display);
        ticker.attach(UPDATE_INTERVAL_SECS, setReadyForUpdate);
}

void loop() {
        if (readyForUpdate && ui.getUiState()->frameState == FIXED) {
                updateData(&display);
        }

        int timeBudget = ui.update();
        if (timeBudget > 0) {
                long start_t = millis();
                // do work
                server.handleClient();
                processEncoder();
                // recalcuate time budget
                Alarm.delay(millis() - start_t);
        }
}

void handleRoot() {
        String out = "<svg width=\"256px\" height=\"252px\" viewBox=\"0 0 256 252\" version=\"1.1\" xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" preserveAspectRatio=\"xMidYMid\"> <defs> <linearGradient x1=\"46.6458496%\" y1=\"-2.08864266%\" x2=\"57.3625907%\" y2=\"137.781163%\" id=\"linearGradient-1\"> <stop stop-color=\"#FF992E\" offset=\"0%\"></stop> <stop stop-color=\"#FF4C2E\" offset=\"63.5%\"></stop><stop stop-color=\"#E3302E\" offset=\"100%\"></stop> </linearGradient> </defs><g><path d=\"M51.5348709,251.616129 C36.4950545,251.616129 23.7864096,240.03547 21.6808353,225.29645 C21.4552381,223.190876 0.625092369,34.7419759 0.625092369,32.6364016 C0.399495123,32.110008 0.0986987952,31.5836145 0.0986987952,31.0572209 L0.148831519,24.740498 C0.148831519,19.4765622 2.20427309,14.2126265 6.41542169,11.0542651 C9.9247122,6.31672289 17.9960803,0 128.012337,0 C237.777931,0 245.924498,6.31672289 249.609253,11.0542651 C253.319074,14.7390201 255.399582,19.4765622 255.399582,25.2668916 L255.324383,30.5308273 C255.324383,31.5836145 255.399582,32.110008 254.873189,33.1627952 L232.513995,225.29645 C232.513995,225.822843 232.238265,226.349237 232.238265,226.875631 C229.506032,241.088257 216.972851,251.616129 202.760225,251.616129 L51.5348709,251.616129 L51.5348709,251.616129 Z\" fill=\"#E5D1BF\"></path><path d=\"M247.553809,24.9510554 C247.553809,21.2663004 246.074894,18.107939 243.96932,16.0023647 C238.52992,9.68564177 188.697994,7.58006747 127.63634,7.58006747 C66.8253484,7.58006747 17.0936891,9.15924819 11.3033598,15.4759711 C9.17271911,18.107939 7.61860474,20.7399068 7.61860474,24.4246618 L7.66873747,30.2149912 C7.66873747,30.7413847 7.61860474,31.2677783 8.14499832,31.7941719 C7.91940107,32.3205655 29.2007413,223.927827 29.2007413,223.927827 C30.7297893,234.982092 40.2550063,243.404389 51.3092714,243.404389 L202.43436,243.404389 C213.964886,243.404389 223.43997,234.982092 224.492757,223.927827 L224.743421,223.927827 L247.303145,31.7941719 L247.303145,30.2149912 L247.303145,24.9510554 L247.553809,24.9510554 L247.553809,24.9510554 Z\" fill=\"#FEF9EC\"></path><ellipse fill=\"#E5D1BF\" cx=\"127.109946\" cy=\"23.3718747\" rx=\"105.805108\" ry=\"5.79032932\"></ellipse><path d=\"M213.964886,205.504051 C213.714223,212.347168 208.70095,217.611104 201.857834,217.611104 L52.2868595,217.611104 C45.7696057,217.611104 40.7813999,212.347168 40.2550063,205.504051 C39.2523519,187.60667 32.8854963,133.914525 32.8854963,133.914525 C32.7350981,129.176983 34.9910706,126.545015 40.2550063,126.545015 L213.964886,126.545015 C218.727494,126.545015 221.334396,129.176983 221.334396,133.914525 C221.484794,133.914525 214.491279,187.60667 213.964886,205.504051 L213.964886,205.504051 L213.964886,205.504051 Z\" fill=\"url(#linearGradient-1)\"></path><path d=\"M97.1556472,136.335936 C94.1476842,140.020691 94.8495422,145.81102 99.0606908,148.969382 L131.747225,174.762667 C135.757843,177.394635 141.69857,176.868241 144.330538,172.657092 L169.848093,139.494297 C172.856057,135.809542 172.229398,130.019213 168.018249,126.860851 L135.256516,101.067566 C131.245898,98.4355984 125.380369,98.961992 122.222008,103.173141 L97.1556472,136.335936 L97.1556472,136.335936 Z\" opacity=\"0.65\" fill=\"#FEF9EC\"></path><path d=\"M65.8226966,112.121831 C60.8094242,112.648225 57.4755984,117.385767 58.001992,122.123309 L63.3160604,163.708402 C64.0680515,168.972337 68.5298635,172.130699 73.7937992,171.604305 L114.7021,166.340369 C119.715372,165.287582 123.274795,161.076434 122.222008,155.812498 L117.208736,114.227406 C116.456745,109.489863 111.694137,105.805108 106.956594,106.857896 L65.8226966,112.121831 L65.8226966,112.121831 Z\" opacity=\"0.65\" fill=\"#FEF9EC\"></path></g></svg>";
        WiFiClient client = server.client();
        if (!client) {
                return;
        }
        Serial.printf("received request from: %s\n", client.remoteIP().toString().c_str());
        server.send ( 200, "image/svg+xml", out);
}

void handleRequest() {
        WiFiClient client = server.client();
        if (!client) {
                return;
        }
        Serial.printf("received request from: %s\n", client.remoteIP().toString().c_str());
        // Serial.printf("args: %d\n", server.args());
        // for(int i = 0; i < server.args(); i++) {
        //         Serial.printf("arg[%d]: %s\n", i, server.arg(i).c_str());
        // }

        String request = server.arg(0);
        int len = request.length();

        JsonStreamingParser parser;
        ScheduleListener scheduleParser;
        parser.setListener(&scheduleParser);

        for(int i = 0; i < len; i++) {
                parser.parse(request.charAt(i));
        }

        Schedule schedule = scheduleParser.getSchedule();
        if(addSchedule(schedule)) {
                Serial.printf("added schedule successfully\n");
        }

        String out = dumpSchedules();
        Serial.printf("schedules: \n%s", out.c_str());
        server.send(200, "application/json", out);
}

void handleDelete() {
        WiFiClient client = server.client();
        if (!client) {
                return;
        }
        Serial.printf("received request from: %s\n", client.remoteIP().toString().c_str());
        // Serial.printf("args: %d\n", server.args());
        // for(int i = 0; i < server.args(); i++) {
        //         Serial.printf("arg[%d]: %s\n", i, server.arg(i).c_str());
        // }
        String request = server.arg(0);
        int len = request.length();

        JsonStreamingParser parser;
        ScheduleListener scheduleParser;
        parser.setListener(&scheduleParser);

        for(int i = 0; i < len; i++) {
                parser.parse(request.charAt(i));
        }

        Schedule schedule = scheduleParser.getSchedule();
        int idx = -1;
        for(int i = 0; i < scheduleIdx; i++) {
                if(schedules[i].alarmId == schedule.alarmId) {
                        idx = i;
                }
        }
        if(idx >= 0) {
                removeSchedule(idx);
        }

        String out = dumpSchedules();
        Serial.printf("schedules: \n%s", out.c_str());
        server.send(200, "application/json", out);
}

Schedule getSchedule(AlarmID_t alarmId) {
  for(int i = 0; i < scheduleIdx; i++) {
    if(schedules[i].alarmId == alarmId) {
      return schedules[i];
    }
  }
  return Schedule();
}

void waterON(AlarmID_t alarmId) {
        Serial.printf("%s water: on", timeFmt(now()).c_str());
        Schedule s = getSchedule(alarmId);
        time_t offTime = s.duration * 60;
        Alarm.alarmOnce(offTime, waterOFF);
        // digitalWrite(s.zone, HIGH);
}

void waterOFF(AlarmID_t AlarmId) {
        Serial.printf("%s water: off", timeFmt(now()).c_str());
        // digitalWrite(s.zone, LOW);
}

String dumpSchedules() {
        String out = "[\n";
        for(int i = 0; i < scheduleIdx; i++) {
                Schedule schedule = schedules[i];
                out += dumpSchedule(schedule);
                if(i < scheduleIdx-1) {
                        out += ",\n";
                }
        }
        out += "]\n";
        return out;
}

String dumpSchedule(Schedule schedule) {
        String out = "{\n";

        out += "\"alarmId\"";
        out += ":\"";
        out += schedule.alarmId;
        out += "\",\n";

        out += "\"zone\"";
        out += ":\"";
        out += schedule.zone;
        out += "\",\n";

        out += "\"dow\"";
        out += ":\"";
        out += schedule.dow;
        out += "\",\n";

        out += "\"duration\"";
        out += ":\"";
        out += schedule.duration;
        out += "\",\n";

        out += "\"hours\"";
        out += ":\"";
        out += schedule.hours;
        out += "\",\n";

        out += "\"mins\"";
        out += ":\"";
        out += schedule.mins;
        out += "\",\n";

        out += "\"secs\"";
        out += ":\"";
        out += schedule.secs;
        out += "\"\n";

        out += "}\n";
        return out;
}

boolean addSchedule(Schedule schedule) {
        if(scheduleIdx + 1 > SCHEDULE_MAX) {
                Serial.printf("Too many schedules; rejecting");
                return false;
        }
        for(int i = 0; i < scheduleIdx; i++) {
                if(schedules[i].zone == schedule.zone) {
                        Serial.printf("zone: %d already has a schedule; rejecting", schedule.zone);
                        return false;
                }
        }
        schedule.alarmId = Alarm.alarmRepeat(getDOW(schedule.dow), schedule.hours, schedule.mins, schedule.secs, waterON);
        if(schedule.alarmId != dtINVALID_ALARM_ID) {
                schedules[scheduleIdx++] = schedule;
        } else {
                Serial.printf("Invalid alarm; rejecting");
                return false;
        }
        Serial.printf("alarm count: %d\n", Alarm.count());
        return true;
}

void removeSchedule(int idx) {
        if(idx > scheduleIdx) {
                Serial.printf("invalid idx: %d\n", idx);
        }
        Schedule schedule = schedules[idx];
        for(int i = idx; i < scheduleIdx-1; i++) {
                schedules[i] = schedules[i+1];
        }
        scheduleIdx -= 1;
        Alarm.free(schedule.alarmId);
        Serial.printf("removed schedule successfully: %s\n", dumpSchedule(schedule).c_str());
        Serial.printf("alarm count: %d\n", Alarm.count());
}

timeDayOfWeek_t getDOW(int dow) {
        return DOW[dow];
}

void rtcSync() {
        setTime(RTC.get()); // comment if no RTC HW
}

void logTime() {
        Serial.printf("date: %s time: %s\n", currentdate().c_str(), currenttime().c_str());
}

void drawProgress(OLEDDisplay *display, int percentage, String label) {
        display->clear();
        display->setTextAlignment(TEXT_ALIGN_CENTER);
        display->setFont(ArialMT_Plain_10);
        display->drawString(64, 10, label);
        display->drawProgressBar(2, 28, 124, 10, percentage);
        display->display();
}

void updateData(OLEDDisplay *display) {
        drawProgress(display, 10, "Updating time...");
        readyForUpdate = false;
        drawProgress(display, 100, "Done...");
        delay(1000);
}

String padDigits(int digits) {
        // utility for digital clock display: prints preceding colon and leading 0
        String digStr = "";
        if (digits < 10) {
                digStr += '0';
        }
        digStr += String(digits);
        return digStr;
}

String currenttime() {
        return timeFmt(now());
}

String currentdate() {
        return dateFmt(now());
}

String timeFmt(time_t moment) {
        String timeStr = "";
        timeStr += hourFormat12(moment);
        timeStr += ":";
        timeStr += padDigits(minute(moment));
        timeStr += ":";
        timeStr += padDigits(second(moment));
        timeStr += isAM(moment) ? "am" : "pm";
        return timeStr;
}

String dateFmt(time_t moment) {
        String dateStr = "";
        dateStr += year(moment);
        dateStr += "/";
        dateStr += padDigits(month(moment));
        dateStr += "/";
        dateStr += padDigits(day(moment));
        return dateStr;
}

void drawDateTime(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
        display->setTextAlignment(TEXT_ALIGN_CENTER);
        display->setFont(ArialMT_Plain_10);
        String date = currentdate();
        int textWidth = display->getStringWidth(date);
        display->drawString(64 + x, 5 + y, date);
        display->setFont(ArialMT_Plain_24);
        String time = currenttime();
        textWidth = display->getStringWidth(time);
        display->drawString(64 + x, 15 + y, time);
        display->setTextAlignment(TEXT_ALIGN_LEFT);
}

void drawZone1(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
        drawZoneN(1, display, state, x, y);
}

void drawZone2(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
        drawZoneN(2, display, state, x, y);
}

void drawZone3(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
        drawZoneN(3, display, state, x, y);
}

void drawZone4(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
        drawZoneN(4, display, state, x, y);
}

void drawZone5(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
        drawZoneN(5, display, state, x, y);
}

void drawZone6(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
        drawZoneN(6, display, state, x, y);
}

void drawZone7(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
        drawZoneN(7, display, state, x, y);
}

void drawZone8(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
        drawZoneN(8, display, state, x, y);
}

void drawZoneN(int zone, OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
        display->setTextAlignment(TEXT_ALIGN_CENTER);
        display->setFont(ArialMT_Plain_10);
        String text = "Zone " + String(zone);
        int textWidth = display->getStringWidth(text);
        display->drawString(64 + x, 1 + y, text);
        // display->setFont(ArialMT_Plain_10);
        time_t sch = getZoneSchedule(zone);
        int idx = getZoneScheduleIdx(zone);
        Schedule s = schedules[idx];
        display->setFont(ArialMT_Plain_16);
        if(sch > 100) {
                String schedule = timeFmt(sch);
                display->drawString(64 + x, 11 + y, schedule);
                String dow = dayStr(s.dow + 1);
                display->drawString(64 + x, 27 + y, dow);
        } else {
                display->drawString(64 + x, 21 + y, "not set");
        }
        display->setTextAlignment(TEXT_ALIGN_LEFT);

}

int getZoneScheduleIdx(int zone) {
        for(int i = 0; i < scheduleIdx; i++) {
                Schedule s = schedules[i];
                if(s.zone == zone) {
                        return i;
                }
        }
        return -1;
}

time_t getZoneSchedule(int zone) {
        for(int i = 0; i < scheduleIdx; i++) {
                Schedule s = schedules[i];
                if(s.zone == zone) {
                        return Alarm.read(s.alarmId);
                }
        }
        return 0;
}

void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState* state) {
        display->setColor(WHITE);
        display->setFont(ArialMT_Plain_10);
        display->setTextAlignment(TEXT_ALIGN_LEFT);
        String ctime = currenttime();
        display->drawString(0, 52, ctime);
        String cdate = currentdate();
        display->drawString(62, 52, cdate);

        int8_t quality = getWifiQuality();
        for (int8_t i = 0; i < 4; i++) {
                for (int8_t j = 0; j < 2 * (i + 1); j++) {
                        if (quality > i * 25 || j == 0) {
                                display->setPixel(120 + 2 * i, 63 - j);
                        }
                }
        }

        display->drawHorizontalLine(0, 50, 128);
}

// converts the dBm to a range between 0 and 100%
int8_t getWifiQuality() {
        int32_t dbm = WiFi.RSSI();
        if(dbm <= -100) {
                return 0;
        } else if(dbm >= -50) {
                return 100;
        } else {
                return 2 * (dbm + 100);
        }
}

void setReadyForUpdate() {
        Serial.println("Setting readyForUpdate to true");
        readyForUpdate = true;
}

void processEncoder() {
        long position = enc.read();
        if (position != encPosition) {
                encPosition = position;
                if(lastPosition != encPosition) {
                        reactDisplay();
                        lastPosition = encPosition;
                }
        }
}

void reactDisplay() {
        if(encPosition > lastPosition) {
                ui.nextFrame();
                ui.setAutoTransitionForwards();
        } else if(encPosition < lastPosition) {
                ui.previousFrame();
                ui.setAutoTransitionBackwards();
        }
}
