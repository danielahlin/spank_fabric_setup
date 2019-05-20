# $Header: $

CC=gcc
CFLAGS= -fPIC -I /afs/pdc.kth.se/roots/srv/ttest/v1.2/usr/include
LD=gcc
LDFLAGS=-shared
DEST=/afs/pdc.kth.se/cluster/ttest/usr/lib64/slurm

all: spank_fabric_setup.so

spank_fabric_setup.o: spank_fabric_setup.c
	$(CC) $(CFLAGS) -c -o $@ $^

spank_fabric_setup.so: spank_fabric_setup.o
	$(LD) -o $@ $(LDFLAGS) $^

install: spank_fabric_setup.so spank_fabric_partition_checker
	cp spank_fabric_setup.so $(DEST)/spank_fabric_setup.so
	cp spank_fabric_partition_checker /afs/pdc.kth.se/cluster/ttest/etc/slurm/spank_fabric_partition_checker
	cp spank_fabric_setup.sh /afs/pdc.kth.se/cluster/ttest/etc/slurm/prologslurmctld.d/spank_fabric_setup.sh

clean:
	rm -f spank_fabric_setup.o spank_fabric_setup.so
