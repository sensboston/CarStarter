# Smart Car Starter project

![alt text](https://github.com/sensboston/CarStarter/blob/master/car_starter.png?raw=true)

Pretty simple (by hardware) but "smart enough" project to implement one of the important home automation tasks.
If you should commute every cold winter morning to your office, and do have car remote start option installed, this solution is definitely for you!

No proprietary, closed sources "smart gadgets" in your car, no monthly contracts: everything is simple, free and open source.

What do you need to build this project:
- ESP8266 SoC: https://www.amazon.com/ESP8266-Network-Development-ESP-12E-Internet/dp/B07TXQGZZF/ 
- MG995 servo motor: https://www.amazon.com/gp/product/B08C7HTB2S/
- a couple of libraries (not available from Arduino IDE for download):
  - https://github.com/gmag11/ESPNtpClient
  - https://github.com/dancol90/ESP8266Ping
- soldering iron
- some tools 
- skillful hands

I still don't have 3D printer, so to build a solution base, I've used half-inch wood plank, saw, jigsaw, drill and screws.
The most challenging part of this project was a crank mechanism (to convert rotation to linear motion). I couldn't find any option available on Internet fopr reasonable price, so I built my own using plastic tube and rod from broken windspinner. Using lighter, pliers, needle file, drill and plastic bolts, I've got perfectly working crank. I've attach my crank to base by epoxy glue, and everything become "stable as a rock".

I created CarStarter.ino app for the ESP8266 devboard (however I prefer "standard" ESP32 much more - but on moment to strating working on the project, I had ESP8266 only) but app can be simple ported to ESP32, or any Arduino-compatible SoC.

Web UI screenshot from desktop Chrome browser:

![alt text](https://github.com/sensboston/CarStarter/blob/master/chrome_screenshot.png?raw=true)

Web UI screenshot from mobile Chrome browser:

![alt text](https://github.com/sensboston/CarStarter/blob/master/mobile_screenshot.png?raw=true)
