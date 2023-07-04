
OBJS= orvibo_plug.o orvibo.o
LIBOJS=

SHARE=/usr/local/share/house

# Local build ---------------------------------------------------

all: orvibo orvibosetup

clean:
	rm -f *.o *.a orvibo

rebuild: clean all

%.o: %.c
	gcc -c -Os -o $@ $<

orvibo: $(OBJS)
	gcc -Os -o orvibo $(OBJS) -lhouseportal -lechttp -lssl -lcrypto -lgpiod -lrt

orvibosetup: orvibosetup.o
	gcc -Os -o orvibosetup orvibosetup.o

# Distribution agnostic file installation -----------------------

install-files:
	mkdir -p /usr/local/bin
	mkdir -p /var/lib/house
	mkdir -p /etc/house
	rm -f /usr/local/bin/orvibo
	cp orvibo /usr/local/bin
	chown root:root /usr/local/bin/orvibo
	chmod 755 /usr/local/bin/orvibo
	mkdir -p $(SHARE)/public/orvibo
	chmod 755 $(SHARE) $(SHARE)/public $(SHARE)/public/orvibo
	cp public/* $(SHARE)/public/orvibo
	chown root:root $(SHARE)/public/orvibo/*
	chmod 644 $(SHARE)/public/orvibo/*
	touch /etc/default/orvibo

uninstall-files:
	rm -rf $(SHARE)/public/orvibo
	rm -f /usr/local/bin/orvibo

purge-config:
	rm -rf /etc/house/orvibo.config /etc/default/orvibo

# Distribution agnostic systemd support -------------------------

install-systemd:
	cp systemd.service /lib/systemd/system/orvibo.service
	chown root:root /lib/systemd/system/orvibo.service
	systemctl daemon-reload
	systemctl enable orvibo
	systemctl start orvibo

uninstall-systemd:
	if [ -e /etc/init.d/orvibo ] ; then systemctl stop orvibo ; systemctl disable orvibo ; rm -f /etc/init.d/orvibo ; fi
	if [ -e /lib/systemd/system/orvibo.service ] ; then systemctl stop orvibo ; systemctl disable orvibo ; rm -f /lib/systemd/system/orvibo.service ; systemctl daemon-reload ; fi

stop-systemd: uninstall-systemd

# Debian GNU/Linux install --------------------------------------

install-debian: stop-systemd install-files install-systemd

uninstall-debian: uninstall-systemd uninstall-files

purge-debian: uninstall-debian purge-config

# Void Linux install --------------------------------------------

install-void: install-files

uninstall-void: uninstall-files

purge-void: uninstall-void purge-config

# Default install (Debian GNU/Linux) ----------------------------

install: install-debian

uninstall: uninstall-debian

purge: purge-debian

