/* Orvibo - A simple home web server for control of Orvibo WiFi plugs.
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
 * orvibo.c - Main loop of the orvibo program.
 *
 * SYNOPSYS:
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "echttp.h"
#include "echttp_cors.h"
#include "echttp_json.h"
#include "echttp_static.h"
#include "houseportalclient.h"
#include "housediscover.h"
#include "houselog.h"
#include "houseconfig.h"
#include "housedepositor.h"

#include "orvibo_plug.h"

static int use_houseportal = 0;
static char HostName[256];

static void hc_help (const char *argv0) {

    int i = 1;
    const char *help;

    printf ("%s [-h] [-debug] [-test]%s\n", argv0, echttp_help(0));

    printf ("\nGeneral options:\n");
    printf ("   -h:              print this help.\n");

    printf ("\nHTTP options:\n");
    help = echttp_help(i=1);
    while (help) {
        printf ("   %s\n", help);
        help = echttp_help(++i);
    }
    exit (0);
}

static const char *orvibo_status (const char *method, const char *uri,
                                  const char *data, int length) {
    static char buffer[65537];
    ParserToken token[1024];
    char pool[65537];
    char host[256];
    int count = orvibo_plug_count();
    int i;

    gethostname (host, sizeof(host));

    ParserContext context = echttp_json_start (token, 1024, pool, 65537);

    int root = echttp_json_add_object (context, 0, 0);
    echttp_json_add_string (context, root, "host", host);
    echttp_json_add_string (context, root, "proxy", houseportal_server());
    echttp_json_add_integer (context, root, "timestamp", (long)time(0));
    int top = echttp_json_add_object (context, root, "control");
    int container = echttp_json_add_object (context, top, "status");

    for (i = 0; i < count; ++i) {
        time_t pulsed = orvibo_plug_deadline(i);
        const char *name = orvibo_plug_name(i);
        const char *status = orvibo_plug_failure(i);
        if (!status) status = orvibo_plug_get(i)?"on":"off";
        const char *commanded = orvibo_plug_commanded(i)?"on":"off";

        int point = echttp_json_add_object (context, container, name);
        echttp_json_add_string (context, point, "state", status);
        echttp_json_add_string (context, point, "command", commanded);
        if (pulsed)
            echttp_json_add_integer (context, point, "pulse", (int)pulsed);
        echttp_json_add_string (context, point, "gear", "light");
    }
    const char *error = echttp_json_export (context, buffer, 65537);
    if (error) {
        echttp_error (500, error);
        return "";
    }
    echttp_content_type_json ();
    return buffer;
}

static const char *orvibo_set (const char *method, const char *uri,
                               const char *data, int length) {

    const char *point = echttp_parameter_get("point");
    const char *statep = echttp_parameter_get("state");
    const char *pulsep = echttp_parameter_get("pulse");
    int state;
    int pulse;
    int i;
    int count = orvibo_plug_count();
    int found = 0;

    if (!point) {
        echttp_error (404, "missing point name");
        return "";
    }
    if (!statep) {
        echttp_error (400, "missing state value");
        return "";
    }
    if ((strcmp(statep, "on") == 0) || (strcmp(statep, "1") == 0)) {
        state = 1;
    } else if ((strcmp(statep, "off") == 0) || (strcmp(statep, "0") == 0)) {
        state = 0;
    } else {
        echttp_error (400, "invalid state value");
        return "";
    }

    pulse = pulsep ? atoi(pulsep) : 0;
    if (pulse < 0) {
        echttp_error (400, "invalid pulse value");
        return "";
    }

    for (i = 0; i < count; ++i) {
       if ((strcmp (point, "all") == 0) ||
           (strcmp (point, orvibo_plug_name(i)) == 0)) {
           found = 1;
           orvibo_plug_set (i, state, pulse);
       }
    }

    if (! found) {
        echttp_error (404, "invalid point name");
        return "";
    }
    return orvibo_status (method, uri, data, length);
}

static const char *orvibo_config (const char *method, const char *uri,
                                  const char *data, int length) {

    if (strcmp ("GET", method) == 0) {
        static char buffer[65537];
        orvibo_plug_live_config (buffer, sizeof(buffer));
        echttp_content_type_json ();
        return buffer;
    } else if (strcmp ("POST", method) == 0) {
        const char *error = houseconfig_update(data);
        if (error) {
            echttp_error (400, error);
        } else {
            orvibo_plug_refresh();
            houselog_event ("SYSTEM", "CONFIG", "SAVE", "TO DEPOT %s", houseconfig_name());
            housedepositor_put ("config", houseconfig_name(), data, length);
        }
    } else {
        echttp_error (400, "invalid method");
    }
    return "";
}

static void orvibo_background (int fd, int mode) {

    static time_t LastRenewal = 0;
    time_t now = time(0);

    if (use_houseportal) {
        static const char *path[] = {"control:/orvibo"};
        if (now >= LastRenewal + 60) {
            if (LastRenewal > 0)
                houseportal_renew();
            else
                houseportal_register (echttp_port(4), path, 1);
            LastRenewal = now;
        }
    }
    orvibo_plug_periodic(now);
    housediscover (now);
    houselog_background(now);
    housedepositor_periodic (now);
}

static void orvibo_config_listener (const char *name, time_t timestamp,
                                    const char *data, int length) {

    houselog_event ("SYSTEM", "CONFIG", "LOAD", "FROM DEPOT %s", name);
    if (!houseconfig_update (data)) orvibo_plug_refresh();
}

static void orvibo_protect (const char *method, const char *uri) {
    echttp_cors_protect(method, uri);
}

int main (int argc, const char **argv) {

    const char *error;

    // These strange statements are to make sure that fds 0 to 2 are
    // reserved, since this application might output some errors.
    // 3 descriptors are wasted if 0, 1 and 2 are already open. No big deal.
    //
    open ("/dev/null", O_RDONLY);
    dup(open ("/dev/null", O_WRONLY));

    signal(SIGPIPE, SIG_IGN);

    gethostname (HostName, sizeof(HostName));

    echttp_default ("-http-service=dynamic");

    argc = echttp_open (argc, argv);
    if (echttp_dynamic_port()) {
        houseportal_initialize (argc, argv);
        use_houseportal = 1;
    }
    housediscover_initialize (argc, argv);
    houselog_initialize ("orvibo", argc, argv);
    housedepositor_initialize (argc, argv);

    houseconfig_default ("-config=orvibo");
    error = houseconfig_load (argc, argv);
    if (error) {
        houselog_trace
            (HOUSE_FAILURE, "CONFIG", "Cannot load configuration: %s", error);
    }
    error = orvibo_plug_initialize (argc, argv);
    if (error) {
        houselog_trace
            (HOUSE_FAILURE, "PLUG", "Cannot initialize: %s", error);
        exit(1);
    }
    housedepositor_subscribe ("config", houseconfig_name(), orvibo_config_listener);

    echttp_cors_allow_method("GET");
    echttp_protect (0, orvibo_protect);

    echttp_route_uri ("/orvibo/status", orvibo_status);
    echttp_route_uri ("/orvibo/set",    orvibo_set);

    echttp_route_uri ("/orvibo/config", orvibo_config);

    echttp_static_route ("/", "/usr/local/share/house/public");
    echttp_background (&orvibo_background);
    houselog_event ("SERVICE", "orvibo", "START", "ON %s", HostName);
    echttp_loop();
}

