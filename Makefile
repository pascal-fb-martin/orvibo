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
#
# WARNING
# 
# This Makefile depends on echttp and houseportal (dev) being installed.

prefix=/usr/local
SHARE=$(prefix)/share/house
        
INSTALL=/usr/bin/install

HAPP=orvibo
HCAT=automation

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
	gcc -Os -o orvibo $(OBJS) -lhouseportal -lechttp -lssl -lcrypto -lmagic -lrt

orvibosetup: orvibosetup.o
	gcc -Os -o orvibosetup orvibosetup.o

# Distribution agnostic file installation -----------------------

install-ui: install-preamble
	$(INSTALL) -m 0755 -d $(DESTDIR)$(SHARE)/public/orvibo
	$(INSTALL) -m 0644 public/* $(DESTDIR)$(SHARE)/public/orvibo

install-runtime: install-preamble
	$(INSTALL) -m 0755 -s orvibo $(DESTDIR)$(prefix)/bin
	touch $(DESTDIR)/etc/default/orvibo

install-app: install-ui install-runtime

uninstall-app:
	rm -rf $(DESTDIR)$(SHARE)/public/orvibo
	rm -f $(DESTDIR)$(prefix)/bin/orvibo

purge-app:

purge-config:
	rm -f $(DESTDIR)/etc/house/orvibo.config
	rm -f $(DESTDIR)/etc/default/orvibo

# Build a private Debian package. -------------------------------

install-package: install-ui install-runtime install-systemd

debian-package: debian-package-generic

# System installation. ------------------------------------------

include $(SHARE)/install.mak

