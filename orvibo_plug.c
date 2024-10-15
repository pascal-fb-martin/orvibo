/* orvibo - A simple home web server for control of orvibo WiFi plugs
 *
 * Copyright 2020, Pascal Martin
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 *
 * orvibo_plug.c - Control an Orvibo WiFi plug or switch.
 *
 * SYNOPSYS:
 *
 * const char *orvibo_plug_configure (int argc, const char **argv);
 *
 *    Retrieve the configuration and initialize access to the plugs.
 *    Return the number of configured relay points available.
 *
 * const char *orvibo_plug_refresh (void);
 *
 *    Re-evaluate the GPIO setup after the configuration changed.
 *
 * int orvibo_plug_count (void);
 *
 *    Return the number of configured relay points available.
 *
 * const char *orvibo_plug_name (int point);
 *
 *    Return the name of an orvibo plug.
 *
 * const char *orvibo_plug_failure (int point);
 *
 *    Return a string describing the failure, or a null pointer if healthy.
 *
 * int    orvibo_plug_commanded (int point);
 * time_t orvibo_plug_deadline (int point);
 *
 *    Return the last commanded state, or the command deadline, for
 *    the specified orvibo plug.
 *
 * int orvibo_plug_get (int point);
 *
 *    Get the actual state of the plug.
 *
 * int orvibo_plug_set (int point, int state, int pulse);
 *
 *    Set the specified point to the on (1) or off (0) state for the pulse
 *    length specified. The pulse length is in seconds. If pulse is 0, the
 *    plug is maintained to the requested state until a new state is issued.
 *
 *    Return 1 on success, 0 if the plug is not known and -1 on error.
 *
 * void orvibo_plug_periodic (void);
 *
 *    This function must be called every second. It runs the Orvibo plug
 *    discovery and ends the expired pulses.
 */

#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "echttp.h"
#include "echttp_json.h"
#include "houselog.h"
#include "houseconfig.h"

#include "orvibo_plug.h"

struct PlugMap {
    char name[32];
    char description[256];
    char macaddress[16];
    struct sockaddr_in ipaddress;
    time_t detected;
    int status;
    int commanded;
    time_t deadline;
};

static struct PlugMap *Plugs;
static int PlugsCount = 0;
static int PlugsSpace = 0;

static int OrviboSocket = -1;
static struct sockaddr_in OrviboBroadcast;

int orvibo_plug_count (void) {
    return PlugsCount;
}

const char *orvibo_plug_name (int point) {
    if (point < 0 || point > PlugsCount) return 0;
    return Plugs[point].name;
}

int orvibo_plug_commanded (int point) {
    if (point < 0 || point > PlugsCount) return 0;
    return Plugs[point].commanded;
}

time_t orvibo_plug_deadline (int point) {
    if (point < 0 || point > PlugsCount) return 0;
    return Plugs[point].deadline;
}

const char *orvibo_plug_failure (int point) {
    if (point < 0 || point > PlugsCount) return 0;
    if (!Plugs[point].detected) return "silent";
    return 0;
}

int orvibo_plug_get (int point) {
    if (point < 0 || point > PlugsCount) return 0;
    return Plugs[point].status;
}

static void orvibo_plug_socket (void) {

    static int OrviboPort = 10000;

    OrviboBroadcast.sin_family = AF_INET;
    OrviboBroadcast.sin_port = htons(OrviboPort);
    OrviboBroadcast.sin_addr.s_addr = INADDR_ANY;

    OrviboSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (OrviboSocket < 0) {
        houselog_trace (HOUSE_FAILURE, "PLUG",
                        "cannot open UDP socket: %s", strerror(errno));
        exit(1);
    }

    if (bind(OrviboSocket,
             (struct sockaddr *)(&OrviboBroadcast),
             sizeof(OrviboBroadcast)) < 0) {
        houselog_trace (HOUSE_FAILURE, "PLUG",
                        "cannot bind to UDP port %d: %s",
                        OrviboPort, strerror(errno));
        exit(1);
    }

    int value = 1;
    if (setsockopt(OrviboSocket, SOL_SOCKET, SO_BROADCAST, &value, sizeof(value)) < 0) {
        houselog_trace (HOUSE_FAILURE, "PLUG",
                        "cannot broadcast: %s", strerror(errno));
        exit(1);
    }
    OrviboBroadcast.sin_addr.s_addr = INADDR_BROADCAST;
    houselog_trace (HOUSE_INFO, "PLUG",
                    "UDP port %d is now open", OrviboPort);
}

static unsigned char hex2bin(char data) {
    if (data >= '0' && data <= '9')
        return data - '0';
    if (data >= 'a' && data <= 'f')
        return data - 'a' + 10;
    if (data >= 'A' && data <= 'F')
        return data - 'A' + 10;
    return 0;
}

static char bin2hex (unsigned char d) {
    d &= 0x0f;
    if (d >= 0 && d <= 9) return '0' + d;
    if (d >= 10 && d <= 15) return ('a' - 10) + d;
    return '0';
}

static void orvibo_plug_send (const struct sockaddr_in *a, const char *d) {
    if (echttp_isdebug())
        printf ("Sending %s%s\n", d, (a==&OrviboBroadcast)?" (broadcast)":"");
    static unsigned char buffer[1500];
    int i;
    for (i = 0; d[i] != 0; i += 2) {
        buffer[i/2] = hex2bin(d[i]) * 16 + hex2bin(d[i+1]);
    }
    int sent = sendto (OrviboSocket, buffer, i/2, 0,
                       (struct sockaddr *)a, sizeof(struct sockaddr_in));
    if (sent < 0)
        houselog_trace
            (HOUSE_FAILURE, "PLUG", "sendto() error: %s", strerror(errno));
}

static void orvibo_plug_sense (void) {
    orvibo_plug_send (&OrviboBroadcast, "686400067161");
}

static void orvibo_plug_subscribe (int plug) {
    static char subscribe[] =
        "6864001e636cFFFFFFFFFFFF202020202020FFFFFFFFFFFF202020202020";
    const char *mac = Plugs[plug].macaddress;
    int i, j, k;
    for (i = 0, j = 12, k = 46; i < 12; i += 2, j += 2, k -= 2) {
        subscribe[j] = subscribe[k] = mac[i];
        subscribe[j+1] = subscribe[k+1] = mac[i+1];
    }
    orvibo_plug_send (&(Plugs[plug].ipaddress), subscribe);
}

static void orvibo_plug_control (int plug, int state) {
    static char command[] =
        "686400176463FFFFFFFFFFFF202020202020000000000F";
    const char *mac = Plugs[plug].macaddress;
    int i, j;
    for (i = 0, j = 12; i < 12; i += 2, j += 2) {
        command[j] = mac[i];
        command[j+1] = mac[i+1];
    }
    command[strlen(command)-1] = state?'1':'0';
    orvibo_plug_send (&(Plugs[plug].ipaddress), command);
}

int orvibo_plug_set (int point, int state, int pulse) {

    const char *namedstate = state?"on":"off";

    if (point < 0 || point > PlugsCount) return 0;

    if (echttp_isdebug()) {
        if (pulse) fprintf (stderr, "set %s to %s at %ld (pulse %ds)\n", Plugs[point].name, namedstate, time(0), pulse);
        else       fprintf (stderr, "set %s to %s at %ld\n", Plugs[point].name, namedstate, time(0));
    }

    if (pulse > 0) {
        Plugs[point].deadline = time(0) + pulse;
        houselog_event ("DEVICE", Plugs[point].name, "SET",
                        "%s FOR %d SECONDS", namedstate, pulse);
    } else {
        Plugs[point].deadline = 0;
        houselog_event ("DEVICE", Plugs[point].name, "SET", "%s", namedstate);
    }
    Plugs[point].commanded = state;

    // Only send a command if we detected the device on the network.
    //
    if (Plugs[point].detected) {
        orvibo_plug_subscribe (point);
        orvibo_plug_control (point, state);
    }
}

void orvibo_plug_periodic (time_t now) {

    static time_t LastRetry = 0;
    static time_t LastSense = 0;
    int i;

    if (now >= LastSense + 30) {
        LastSense = now;
        orvibo_plug_sense ();
    }

    if (now < LastRetry + 5) return;
    LastRetry = now;

    for (i = 0; i < PlugsCount; ++i) {
        // If we did not detect a plug for 3 senses, consider it failed.
        if (Plugs[i].detected > 0 && Plugs[i].detected < now - 90) {
            houselog_event ("DEVICE", Plugs[i].name, "SILENT",
                            "MAC ADDRESS %s", Plugs[i].macaddress);
            Plugs[i].detected = 0;
        }

        if (Plugs[i].deadline > 0 && now >= Plugs[i].deadline) {
            houselog_event ("DEVICE", Plugs[i].name, "RESET", "END OF PULSE");
            Plugs[i].commanded = 0;
            Plugs[i].deadline = 0;
        }
        if (Plugs[i].status != Plugs[i].commanded) {
            if (Plugs[i].detected) {
                const char *state = Plugs[i].commanded?"on":"off";
                houselog_event ("DEVICE", Plugs[i].name, "RETRY", state);
                orvibo_plug_subscribe (i);
                orvibo_plug_control (i, Plugs[i].commanded);
            }
        }
    }
}

const char *orvibo_plug_refresh (void) {

    int i;
    int plugs;

    for (i = 0; i < PlugsCount; ++i) {
        Plugs[i].name[0] = 0;
        Plugs[i].macaddress[0] = 0;
        Plugs[i].description[0] = 0;
        Plugs[i].deadline = 0;
        Plugs[i].detected = 0;
    }
    PlugsCount = 0;

    if (houseconfig_active()) {
        plugs = houseconfig_array (0, ".orvibo.plugs");
        if (plugs < 0) return "cannot find plugs array";

        PlugsCount = houseconfig_array_length (plugs);
        if (echttp_isdebug()) fprintf (stderr, "found %d plugs\n", PlugsCount);
    }
    PlugsSpace = PlugsCount + 32;

    Plugs = calloc(sizeof(struct PlugMap), PlugsSpace);
    if (!Plugs) return "no more memory";

    for (i = 0; i < PlugsCount; ++i) {
        int plug;
        char path[128];
        snprintf (path, sizeof(path), "[%d]", i);
        plug = houseconfig_object (plugs, path);
        if (plug <= 0) continue;
        const char *name = houseconfig_string (plug, ".name");
        if (name) {
            strncpy (Plugs[i].name, name, sizeof(Plugs[i].name));
            Plugs[i].name[sizeof(Plugs[i].name)-1] = 0;
        }
        const char *mac = houseconfig_string (plug, ".address");
        if (mac)
            strncpy (Plugs[i].macaddress, mac, sizeof(Plugs[i].macaddress));
        const char *desc = houseconfig_string (plug, ".description");
        if (desc)
            snprintf (Plugs[i].description, sizeof(Plugs[i].description), "%s", desc);
        if (echttp_isdebug()) fprintf (stderr, "found plug %s, address %s\n", Plugs[i].name, Plugs[i].macaddress);
        Plugs[i].commanded = 0;
        Plugs[i].deadline = 0;
    }
    return 0;
}

const char *orvibo_plug_live_config (char *buffer, int size) {

    static char pool[65537];
    static ParserToken token[1024];
    ParserContext context = echttp_json_start (token, 1024, pool, sizeof(pool));

    int i;

    int root = echttp_json_add_object (context, 0, 0);
    int top = echttp_json_add_object (context, root, "orvibo");
    int plugs = echttp_json_add_array (context, top, "plugs");

    for (i = 0; i < PlugsCount; ++i) {
        if (Plugs[i].name[0] == 0 || Plugs[i].macaddress[0] == 0) continue;
        int plug = echttp_json_add_object (context, plugs, 0);
        echttp_json_add_string (context, plug, "name", Plugs[i].name);
        echttp_json_add_string (context, plug, "address", Plugs[i].macaddress);
        echttp_json_add_string
            (context, plug, "description", Plugs[i].description);
    }
    return echttp_json_export (context, buffer, size);
}

static int binary_equal (const unsigned char *a, const unsigned char *b, int size) {
    while (--size >= 0) {
        if (a[size] != b[size]) return 0;
    }
    return 1;
}

static void importmac (char *mac, const unsigned char *data, int start) {
    int i, j;
    for (i = start + 5, j = 10; i >= start; --i, j -= 2) {
        mac[j] = bin2hex(data[i] >> 4);
        mac[j+1] = bin2hex(data[i]);
    }
    mac[12] = 0;
}

static int orvibo_plug_mac_search (const char *mac) {
    int i;
    for (i = 0; i < PlugsCount; ++i) {
        if (!strcmp(mac, Plugs[i].macaddress)) return i;
    }
    return -1;
}

static void orvibo_plug_dump (unsigned char *d, int l) {
    char buffer[256];
    int i, j;

    if (l >= sizeof(buffer) / 2) l = (sizeof(buffer) / 2) - 1;
    buffer[l*2] = 0;
    for (i = l-1, j = (l-1)*2; i >= 0; --i, j -= 2) {
        buffer[j] = bin2hex(d[i]>>4);
        buffer[j+1] = bin2hex(d[i]);
    }
    fprintf (stderr, "received: %s\n", buffer);
}

static void orvibo_plug_receive (int fd, int mode) {

    static unsigned char discovery[] = {0x68, 0x64, 0, 0x2a, 0x71, 0x61, 0};
    static unsigned char command[] = {0x68, 0x64, 0, 0x17, 0x73};

    unsigned char data[128];
    struct sockaddr_in addr;
    int addrlen = sizeof(addr);

    int size = recvfrom (OrviboSocket, data, sizeof(data), 0,
                         (struct sockaddr *)(&addr), &addrlen);
    if (size > 0) {
        if (echttp_isdebug()) orvibo_plug_dump (data, size);

        char mac[16];
        int macstart = 0;
        int statepos = 0;
        int plug;
        if (binary_equal(discovery, data, sizeof(discovery))) {
            macstart = 7;
            statepos = 41;
        } else if (binary_equal(command, data, sizeof(command))) {
            macstart = 6;
            statepos = 22;
        } else {
            return; // Don't do anything with unused data.
        }
        importmac (mac, data, macstart);
        plug = orvibo_plug_mac_search (mac);
        if (plug < 0 && PlugsCount < PlugsSpace) {
            if (echttp_isdebug()) fprintf (stderr, "new device %s\n", mac);
            plug = PlugsCount++;
            snprintf (Plugs[plug].name, sizeof(Plugs[0].name), "plug%d", plug);
            snprintf (Plugs[plug].macaddress, sizeof(Plugs[0].macaddress),
                      "%s", mac);
            snprintf (Plugs[plug].description, sizeof(Plugs[0].description),
                      "autogenerated");
            houselog_event ("DEVICE", Plugs[plug].name, "ADDED",
                            "MAC ADDRESS %s", mac);
            Plugs[plug].detected = time(0); // Skip the "DETECTED" event.
        }
        if (plug >= 0) {
            if (!Plugs[plug].detected)
                houselog_event ("DEVICE", Plugs[plug].name, "DETECTED",
                                "MAC ADDRESS %s", mac);
            Plugs[plug].detected = time(0);

            int status = (data[statepos] == 1);
            if (Plugs[plug].status != status) {
                houselog_event ("DEVICE", Plugs[plug].name, "CHANGED",
                                "FROM %s TO %s",
                                Plugs[plug].status?"on":"off",
                                status?"on":"off");
                Plugs[plug].status = status;
            }

            memcpy (&(Plugs[plug].ipaddress),
                    &addr, sizeof(Plugs[plug].ipaddress));
        }
    }
}

const char *orvibo_plug_initialize (int argc, const char **argv) {
    orvibo_plug_socket ();
    echttp_listen (OrviboSocket, 1, orvibo_plug_receive, 0);
    return orvibo_plug_refresh ();
}

