# orvibo
A House web service to read and control Orvibo S20 WiFi plugs
## Overview
This is a web server to give access to Ovibo WiFi electric plugs. This server can sense the current status and control the state of each plug. The web API is meant to be compatible with the House control API (e.g. the same web API as [houserelays](https://github.com/pascal-fb-martin/houserelays)).
## Warning
The Orvibo S20 is a discontinued model. Newer Orvibo models do not use the same protocol and this program is not compatible with them.
## Installation
* Install the OpenSSL development package(s).
* Install [echttp](https://github.com/pascal-fb-martin/echttp).
* Install [houseportal](https://github.com/pascal-fb-martin/houseportal).
* Clone this GitHub repository.
* make
* sudo make install
* Edit /etc/house/orvibo.json (see below)
## Configuration
Each plug must be declared in file /etc/house/orvibo.json. A typical example of configuration is:
```
{
    "orvibo" : {
        "plugs" : [
            {
                "name" : "orvibo1",
                "address" : "ACCF239CF008",
                "description" : "one Orvibo plug"
            },
            {
                "name" : "orvibo2",
                "address" : "ACCF239C683C",
                "description" : "another Orvibo plug"
            }
        ]
    }
}
```
## S20 Setup
The web service comes with a small command line tool to configure the Orvibo S20 for the local WiFi network, called orvibosetup:
```
orvibosetup <ssid>
```
The tool first asks for the WiFi password (without echoing it) and then proceeds with the setup. The S20 must already be accessible from the computer (typically if you plan on changing your WiFi password). If it is not, follow the steps below to connect the device:
* Plug the S20 device in a nearby outlet.
* Press the button until it flashes red at high speed, then release the button.
* Press the button again until it flashes blue at high speed, then release the button.
* On your computer, connect to the WiFi SSID "WiWo-S20" (no password).

Once the device and the computer are connected to each other, launch orvibosetup with your WiFi SSID as parameter. When prompted, enter your WiFi password.

The S20 device should reboot: the LED turns off, then blinks red, then shows a steady red (if everything worked fine). The whole sequence may take a minute.

Warning: if you have multiple S20 devices on the network, they will all be impacted. It is not guaranteed this will work, and some device might need to be reprogrammed. It is recommended to disconnect all other S20 devices when setting up one.

Warning: the WiFi password is sent in the clear, possibly through an open WiFi network.

