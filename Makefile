TRACKER=tracker.c readconf.c ssh.c xdr_pars.c
TRACKER_OBJ=dgraph_tracker
UNIT=unit.c xdr_pars.c
UNIT_OBJ=dgraph_unit

all:
	gcc -o $(TRACKER_OBJ) $(TRACKER)
	gcc -o $(UNIT_OBJ) $(UNIT)

install: all
	sudo mkdir -p /etc/dgraph
	sudo cp system.properties /etc/dgraph
	sudo cp $(TRACKER_OBJ) /usr/local/bin
	sudo cp $(UNIT_OBJ) /usr/local/bin
	## sudo useradd -d /home/dgraph -m -s /bin/bash dgraph 2>/dev/null
	## printf dgraph123456\\ndgraph123456\\n|sudo passwd dgraph 2>/dev/null
	sudo mkdir -p /var/log/dgraph
	sudo chown dgraph /var/log/dgraph

run: install
	$(TRACKER_OBJ) < graph3
