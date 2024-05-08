# SmartLight

SmartLight is an intelligent light which gives you various light capabilities, such as a night light or light shows.

The LED lights are controlled by an ESP8266-12F by Alexa via AWS IoT MQTT.  

There is also a PIR (passive infra-red) motion detecting sensor that may be turned on or off that can automatically turn on the lights when motion is detected.

The optional phone app may be installed by scanning the Blynk QR code (QR_Code_BlynkAppSmartlight.png) in the images directory.


# Installing Dependencies

From the aws-mqtt-websockets repo https://github.com/odelot/aws-mqtt-websockets:

Library Name            Location on Internet/Repo                                         Purpose
aws-sdk-arduino         https://github.com/odelot/aws-sdk-arduino                         aws signing functions
arduinoWebSockets       https://github.com/Links2004/arduinoWebSockets                    websocket comm impl
Paho MQTT for Arduino   https://projects.eclipse.org/projects/technology.paho/downloads   mqtt comm impl


To install the aws-mqtt-websockets library do the following :

go to the "libraries" sub-dir of your arduino installation.  In my case this is where mine is:
cd ~/source/arduino/libraries

git clone https://github.com/odelot/aws-mqtt-websockets
git clone https://github.com/odelot/aws-sdk-arduino
git clone https://github.com/Links2004/arduinoWebSockets

Go to the "Paho MQTT for Arduino" site at https://projects.eclipse.org/projects/technology.paho/downloads
scroll down and click on the link "Arduino client library 1.0.0".  This will download "arduino_1.0.0.zip".

Note: you must still be in your arduino libraries dir)

unzip arduino_1.0.0.zip

That's it. Start the Arduino IDE or stop and restart if it was running (the ide will not detect the new libs until restarted.)
