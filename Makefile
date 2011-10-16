CC=g++
CFLAGS=-Wall -g -Os

BOOST_FILESYSTEM=-lboost_filesystem -lboost_system -lboost_regex
CTEMPLATE=-I$(HOME)/local/include -L$(HOME)/local/lib -lctemplate
FFMPEG_CFLAGS = -I/usr/include/ffmpeg
GLIBS=-lgflags -lglog
ID3=-lid3
LIBEVENT=$(shell pkg-config --cflags --libs libevent)
LEVELDB_FLAGS=-I./leveldb/include
LEVELDB=-I./leveldb/include -L./leveldb -lleveldb
YAJL=-lyajl


all: danger build-leveldb

build-leveldb:
	make -C leveldb OPT="-Os"

httpd.o: httpd.cc storage.o
	$(CC) $(CFLAGS) $(LEVELDB_FLAGS) -c $<

storage.o: storage.cc
	$(CC) $(CFLAGS) $(LEVELDB_FLAGS) $(FFMPEG_CFLAGS) -c $^

danger: httpd.o storage.o main.cc
	$(CC) $(CFLAGS) $(CTEMPLATE) $(GLIBS) $(LIBEVENT) $(YAJL) $(LEVELDB) $(ID3) $(BOOST_FILESYSTEM) $^ -o $@

clean:
	rm -f *.o danger
#	make -C leveldb clean

.PHONY: clean build-leveldb
