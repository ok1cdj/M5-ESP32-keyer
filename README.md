# M5-ESP32-keyer

**M5StickC** - ESP32 remote CW keyer by OK1CDJ ondra@ok1cdj.com 

Primary desiged for M5StickC but can be used with **any ESP32 board**

 ![M5](https://raw.githubusercontent.com/ok1cdj/M5-ESP32-keyer/master/M5StickC-key.png)

You can get M5Stack **M5StickC CW** Hat from hamshop.cz

https://www.hamshop.cz/morse-a-cw-c35/m5stack-m5stickc-cw-hat-i402/

If you would like try another ESP32 board remove #define M5Stick and correct Pin assigment

**Features:**

   Responsive Web interface based on Boostrap
   
   ![WEB](https://raw.githubusercontent.com/ok1cdj/M5-ESP32-keyer/master/webinterface.png)
          
   **http** based service for remote control from another application
          
   Simple linux cwdaemon emulation - currently support only, send message, change speed and break trasmition
                                          - tested with Tucnak VHF Contest log http://tucnak.nagano.cz/wiki/Main_Page

  **Usage:**
  
   CW output is on G26 - connect optocoupler or transistor for keying and connect radio
   UDP server listen on 6789
   
   WEB http://url/?apikey=XXXXXXXX when button A is pressed QR code with url is displayed
   
   Config - http://url/cfg or hold button B on start when button A is pressed QR code with url is displayed
   
   
   API http://url/sendmorse?apikey=XXXXXX&message=ok1cdj&speed=33

  **TODO:**
   make compatibility with ESP32 with ethernet like OLIMEX ESP32-POE
   
 [![https://www.buymeacoffee.com/ok1cdj](https://img.shields.io/badge/Donate-Buy%20me%20a%20coffee-orange?style=for-the-badge)](https://www.buymeacoffee.com/ok1cdj)


