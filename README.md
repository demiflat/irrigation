#a simple irrigation controller on a esp8266

###hardware:

esp8266

https://www.aliexpress.com/item/D1-mini-Mini-NodeMcu-4M-bytes-Lua-WIFI-Internet-of-Things-development-board-based-ESP8266-by/32662942091.html

rtc

https://www.aliexpress.com/item/Smart-Electronics-I2C-RTC-DS1307-AT24C32-Real-Time-Clock-Module-For-arduino-AVR-ARM-PIC-Wholesale/32324548167.html

oled

https://www.aliexpress.com/item/Free-shipping-1Pcs-128X64-Blue-OLED-LCD-LED-Display-Module-For-Arduino-0-96-I2C-IIC/32658340632.html

encoder

https://www.aliexpress.com/item/1PCS-FREE-shipping-Rotary-Encoder-Module-Brick-Sensor-Development-for-Dropshipping-KY-040/32648046438.html

###software:
http listener on "/scheduler"
json template:

```javascript
{
    "alarmId":"0",
    "zone":"1",
    "dow":"0",
    "hours":"5",
    "mins":"30",
    "secs":"00",
    "duration":"3"
}
```
*POST creates a schedule

*DELETE deletes a schedule

response is current list of schedules

###dev tool:
http://platformio.org/
