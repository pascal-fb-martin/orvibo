# Orvibo - A simple home web server for control of Orvibo WiFi plugs.
#
# Copyright 2023, Pascal Martin
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor,
# Boston, MA  02110-1301, USA.

HAPP=orvibo
HROOT=/usr/local
SHARE=$(HROOT)/share/house

# Application build ---------------------------------------------

OBJS= orvibo_plug.o orvibo.o
LIBOJS=

all: orvibo orvibosetup

clean:
	rm -f *.o *.a orvibo

rebuild: clean all

%.o: %.c
	gcc -c -Os -o $@ $<

orvibo: $(OBJS)
	gcc -Os -o orvibo $(OBJS) -lhouseportal -lechttp -lssl -lcrypto -lrt

orvibosetup: orvibosetup.o
	gcc -Os -o orvibosetup orvibosetup.o

# Distribution agnostic file installation -----------------------

install-app:
	mkdir -p $(HROOT)/bin
	mkdir -p /var/lib/house
	mkdir -p /etc/house
	rm -f $(HROOT)/bin/orvibo
	cp orvibo $(HROOT)/bin
	chown root:root $(HROOT)/bin/orvibo
	chmod 755 $(HROOT)/bin/orvibo
	mkdir -p $(SHARE)/public/orvibo
	chmod 755 $(SHARE) $(SHARE)/public $(SHARE)/public/orvibo
	cp public/* $(SHARE)/public/orvibo
	chown root:root $(SHARE)/public/orvibo/*
	chmod 644 $(SHARE)/public/orvibo/*
	touch /etc/default/orvibo

uninstall-app:
	rm -rf $(SHARE)/public/orvibo
	rm -f $(HROOT)/bin/orvibo

purge-app:

purge-config:
	rm -rf /etc/house/orvibo.config /etc/default/orvibo

# System installation. ------------------------------------------

include $(SHARE)/install.mak

