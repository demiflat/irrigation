#pragma once

#include <JsonListener.h>
/* test data:
{
"zone":"1",
"dow":"0",
"hours":"5",
"mins":"30",
"secs":"00",
"duration":"3"
}
*/

typedef struct {
        int zone = 0;
        int dow = 0;
        int hours = 0;
        int mins = 0;
        int secs = 0;
        int duration = 0;
        int alarmId = -1;
} Schedule;

class ScheduleListener: public JsonListener {
  private:
    String currentKey;
    int zone = 0;
    int dow = 0;
    int hours = 0;
    int mins = 0;
    int secs = 0;
    int duration = 0;
    int alarmId = 0;

  public:
    virtual void whitespace(char c);

    virtual void startDocument();

    virtual void key(String key);

    virtual void value(String value);

    virtual void endArray();

    virtual void endObject();

    virtual void endDocument();

    virtual void startArray();

    virtual void startObject();

    Schedule getSchedule();
};
