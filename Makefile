
OBJS= orvibo_plug.o orvibo.o
LIBOJS=

SHARE=/usr/local/share/house

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

install:
	if [ -e /etc/init.d/orvibo ] ; then systemctl stop orvibo ; systemctl disable orvibo ; rm -f /etc/init.d/orvibo ; fi
	if [ -e /lib/systemd/system/orvibo.service ] ; then systemctl stop orvibo ; systemctl disable orvibo ; rm -f /lib/systemd/system/orvibo.service ; fi
	mkdir -p /usr/local/bin
	mkdir -p /var/lib/house
	mkdir -p /etc/house
	rm -f /usr/local/bin/orvibo
	cp orvibo /usr/local/bin
	chown root:root /usr/local/bin/orvibo
	chmod 755 /usr/local/bin/orvibo
	cp systemd.service /lib/systemd/system/orvibo.service
	chown root:root /lib/systemd/system/orvibo.service
	mkdir -p $(SHARE)/public/orvibo
	chmod 755 $(SHARE) $(SHARE)/public $(SHARE)/public/orvibo
	cp public/* $(SHARE)/public/orvibo
	chown root:root $(SHARE)/public/orvibo/*
	chmod 644 $(SHARE)/public/orvibo/*
	touch /etc/default/orvibo
	systemctl daemon-reload
	systemctl enable orvibo
	systemctl start orvibo

uninstall:
	systemctl stop orvibo
	systemctl disable orvibo
	rm -rf $(SHARE)/public/orvibo
	rm -f /usr/local/bin/orvibo
	rm -f /lib/systemd/system/orvibo.service /etc/init.d/orvibo
	systemctl daemon-reload

purge: uninstall
	rm -rf /etc/house/orvibo.config /etc/default/orvibo

