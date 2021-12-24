
OBJS= orvibo_plug.o orvibo.o
LIBOJS=

SHARE=/usr/local/share/house

all: orvibo orvibosetup

clean:
	rm -f *.o *.a orvibo

rebuild: clean all

%.o: %.c
	gcc -c -g -O -o $@ $<

orvibo: $(OBJS)
	gcc -g -O -o orvibo $(OBJS) -lhouseportal -lechttp -lssl -lcrypto -lgpiod -lrt

orvibosetup: orvibosetup.o
	gcc -g -O -o orvibosetup orvibosetup.o

install:
	if [ -e /etc/init.d/orvibo ] ; then systemctl stop orvibo ; fi
	mkdir -p /usr/local/bin
	mkdir -p /var/lib/house
	mkdir -p /etc/house
	rm -f /usr/local/bin/orvibo /etc/init.d/orvibo
	cp orvibo /usr/local/bin
	cp init.debian /etc/init.d/orvibo
	chown root:root /usr/local/bin/orvibo /etc/init.d/orvibo
	chmod 755 /usr/local/bin/orvibo /etc/init.d/orvibo
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
	rm -f /usr/local/bin/orvibo /etc/init.d/orvibo
	systemctl daemon-reload

purge: uninstall
	rm -rf /etc/house/orvibo.config /etc/default/orvibo

