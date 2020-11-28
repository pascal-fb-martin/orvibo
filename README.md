# orvibo
A House web service to read and control Orvibo WiFi plugs
## Overview
This is a web server to give access to Ovibo WiFi electric plugs. This server can sense the current status and control the state of each plug. The web API is meant to be compatible with the House control API (e.g. the same web API as [houserelays](https://github.com/pascal-fb-martin/houserelays)).
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
