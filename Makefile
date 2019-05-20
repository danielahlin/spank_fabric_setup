# $Header: $

CC=gcc
SLURMPREFIX:=/usr
CFLAGS= -fPIC -I $(SLURMPREFIX)/include
LD=gcc
LDFLAGS=-shared
LIBDEST:=$(SLURMPREFIX)/lib64/slurm
SCRIPTDEST:=/etc/slurm

all: spank_fabric_setup.so

spank_fabric_setup.o: spank_fabric_setup.c
	$(CC) $(CFLAGS) -c -o $@ $^

spank_fabric_setup.so: spank_fabric_setup.o
	$(LD) -o $@ $(LDFLAGS) $^

install: spank_fabric_setup.so spank_fabric_partition_checker
	cp spank_fabric_setup.so $(LIBDEST)/spank_fabric_setup.so
	cp spank_fabric_partition_checker $(SCRIPTDEST)/spank_fabric_partition_checker
	cp spank_fabric_setup.sh /$(SCRIPTDEST)/prologslurmctld.d/spank_fabric_setup.sh

clean:
	rm -f spank_fabric_setup.o spank_fabric_setup.so
