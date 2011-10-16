CC=g++
CFLAGS=-Wall -g -Os
CTEMPLATE=-I$(HOME)/local/include -L$(HOME)/local/lib -lctemplate
GLIBS=-lgflags -lglog
LIBEVENT=$(shell pkg-config --cflags --libs libevent)

LEVELDB_FLAGS=-I./leveldb/include
LEVELDB=-I./leveldb/include -L./leveldb -lleveldb

YAJL=-lyajl

BOOST_FILESYSTEM=-lboost_filesystem -lboost_system -lboost_regex

all: danger build-leveldb

build-leveldb:
	make -C leveldb OPT="-Os"

httpd.o: httpd.cc storage.o
	$(CC) $(CFLAGS) $(LEVELDB_FLAGS) -c $<

storage.o: storage.cc
	$(CC) $(CFLAGS) $(LEVELDB_FLAGS) -c $^

danger: httpd.o storage.o main.cc
	$(CC) $(CFLAGS) $(CTEMPLATE) $(GLIBS) $(LIBEVENT) $(YAJL) $(LEVELDB) $(BOOST_FILESYSTEM) $^ -o $@

clean:
	rm -f *.o danger
#	make -C leveldb clean

.PHONY: clean build-leveldb
