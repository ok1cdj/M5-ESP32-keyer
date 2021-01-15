# M5-ESP32-keyer

M5StickC - ESP32 remote CW keyer by OK1CDJ ondra@ok1cdj.com 

Primary desiged for M5StickC but can be used with any ESP32 board

If you wold like try another ESP32 board emove #define M5Stick and correct Pin assigment

Features:

   Responsive Web interface based on Boostrap
          
   http based service for remote control from another appication
          
   Simple linux cwdaemon emulation - currently support only, send message, change speed and break trasmition
                                          - tested with Tucnak VHF Contest log http://tucnak.nagano.cz/wiki/Main_Page

  Usage:
  
   CW output is on G26 - connect optocoupler or transistor for keying and connect radio
   UDP server listen on 6789
   
   WEB http://url/?apikey=XXXXXXXX when button A is pressed QR code with url is displayed
   
   Config - http://url/cfg or hold button B on start when button A is pressed QR code with url is displayed
   
   
   API http://url/sendmorse?apikey=XXXXXX&message=ok1cdj&speed=33

  TODO:
   make compatibility with ESP32 with ethernet like OLIMEX ESP32-POE
