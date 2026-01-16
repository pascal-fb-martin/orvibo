/* orvibo - A simple home web server for world domination through Orvibo plugs.
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
 * orvibo_plug.h - An implementation of the Orvibo plug protocol.
 *
 */
void orvibo_plug_initialize (int argc, const char **argv, int livestate);
const char *orvibo_plug_refresh (void);

int orvibo_plug_count (void);
const char *orvibo_plug_name (int point);

const char *orvibo_plug_live_config (char *buffer, int size);

const char *orvibo_plug_failure (int point);

int    orvibo_plug_commanded (int point);
time_t orvibo_plug_deadline  (int point);
int    orvibo_plug_get       (int point);
int    orvibo_plug_set       (int point, int state, int pulse);

void orvibo_plug_periodic (time_t now);

