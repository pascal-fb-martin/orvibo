/* orvibosetup - A simple program to setup an Orvibo S20 wiFi plug.
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
 * orvibosetup.c - Setup an Orvibo S20 WiFi plug or switch.
 *
 * SYNOPSYS:
 *
 * orvibosetup ssid
 *
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

static int OrviboSocket = -1;
static struct sockaddr_in OrviboBroadcast;

static void orvibo_socket (void) {

    static int OrviboPort = 48899;

    OrviboBroadcast.sin_family = AF_INET;
    OrviboBroadcast.sin_port = htons(OrviboPort);
    OrviboBroadcast.sin_addr.s_addr = INADDR_BROADCAST;

    OrviboSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (OrviboSocket < 0) {
        printf ("cannot open UDP socket: %s\n", strerror(errno));
        exit(1);
    }

    int value = 1;
    if (setsockopt(OrviboSocket, SOL_SOCKET, SO_BROADCAST, &value, sizeof(value)) < 0) {
        printf ("cannot broadcast: %s\n", strerror(errno));
        exit(1);
    }
    printf ("UDP socket is ready.\n");
}

static void orvibo_send (const char *d, const char *private) {

    int sent = sendto (OrviboSocket, d, strlen(d), 0,
                       (struct sockaddr *)(&OrviboBroadcast),
                       sizeof(struct sockaddr_in));
    if (sent < 0) {
        printf ("** sendto() error: %s", strerror(errno));
        exit(1);
    }
    if (!private)
        printf ("Sending %s\n", d);
    else {
        char privacy[256];
        strncpy (privacy, d, sizeof(privacy));
        char *p = strstr (privacy, private);
        int i = strlen(private);
        while (--i>=0) *(p++) = '*';
        printf ("Sending %s\n", privacy);
    }
}

static void orvibo_receive (void) {

    char data[128];
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);

    int size = recvfrom (OrviboSocket, data, sizeof(data), 0,
                         (struct sockaddr *)(&addr), &addrlen);

    if (size <= 0) {
        printf ("** recvfrom() error: %s", strerror(errno));
        return;
    }
    printf ("Received: %s\n", data);
}

int main (int argc, char **argv) {

    char buffer[256];

    if (argc != 2) {
       fprintf (stderr, "Invalid parameters: need SSID.\n");
       exit(1);
    }
    snprintf (buffer, sizeof(buffer), "WiFi password for %s? ", argv[1]);
    char *password = strdup(getpass(buffer));

    fflush(stdout);
    
    orvibo_socket ();
    orvibo_send ("HF-A11ASSISTHREAD", 0);
    orvibo_receive();
    orvibo_send ("+ok", 0);
    snprintf (buffer, sizeof(buffer), "AT+WSSSID=%s\r", argv[1]);
    orvibo_send (buffer, 0);
    orvibo_receive();
    snprintf (buffer, sizeof(buffer), "AT+WSKEY=WPA2PSK,AES,%s\r", password);
    orvibo_send (buffer, password);
    orvibo_receive();
    orvibo_send ("AT+WMODE=STA\r", 0);
    orvibo_receive();
    orvibo_send ("AT+Z\r", 0);
    return 0;
}

