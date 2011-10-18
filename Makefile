CC=g++
CFLAGS=-Wall -g -Os

BOOST_FILESYSTEM=-lboost_filesystem -lboost_system -lboost_regex
CTEMPLATE=-I$(HOME)/local/include -L$(HOME)/local/lib -lctemplate
GLIBS=-lgflags -lglog
ID3=-lid3
LIBEVENT=$$(pkg-config --cflags --libs libevent)
LEVELDB_FLAGS=-I./leveldb/include
LEVELDB=-I./leveldb/include -L./leveldb -lleveldb -lpthread -lsnappy
PB=$$(pkg-config --cflags --libs protobuf)
YAJL=-lyajl

all: danger build-leveldb

build-leveldb:
	make -C leveldb OPT="-Os"

protogen:
	mkdir -p protogen

protogen/track.pb.cc: protobuf/track.proto protogen
	protoc -I=protobuf --cpp_out=protogen protobuf/track.proto

protogen/track.pb.h: protobuf/track.proto protogen
	protoc -I=protobuf --cpp_out=protogen protobuf/track.proto

track.pb.o: protogen/track.pb.cc
	gcc -c $^

httpd.o: httpd.cc storage.o
	$(CC) $(CFLAGS) $(LEVELDB_FLAGS) -c $<

storage.o: storage.cc protogen/track.pb.h
	$(CC) $(CFLAGS) $(LEVELDB_FLAGS) -c $<

danger: httpd.o storage.o track.pb.o main.cc
	$(CC) $(CFLAGS) $$(pkg-config icu-i18n --cflags --libs) $(CTEMPLATE) $(GLIBS) $(LIBEVENT) $(YAJL) $(ID3) $(BOOST_FILESYSTEM) $(PB) $^ $(LEVELDB) -o $@

clean:
	rm -f *.o danger
	rm -rf protogen
#	make -C leveldb clean

.PHONY: clean build-leveldb proto
