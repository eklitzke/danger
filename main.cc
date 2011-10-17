#include <iostream>

#include <event2/event.h>
#include <gflags/gflags.h>
#include <glog/logging.h>

#include "httpd.h"
#include "storage.h"
#include "protogen/track.pb.h"

DECLARE_int32(port);
DECLARE_string(iface);
DEFINE_bool(fast, false, "fast startup from leveldb");

using namespace danger;

int main(int argc, char **argv)
{
  GOOGLE_PROTOBUF_VERIFY_VERSION;
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  LOG(INFO) << "starting...";

  Storage storage("/home/evan/Music", "/tmp/danger.db");
  if (FLAGS_fast) {
    storage.update_from_level();
  } else {
    storage.update();
  }

  struct event_base *base = event_base_new();
  if (base == NULL) {
	std::cerr << "failed to event_base_new()" << std::endl;
  }
  initialize_httpd(base, &storage);

  /* start the event loop */
  if (event_base_dispatch(base) != 0) {
    std::cerr << "failed to event_base_dispatch(); check that " <<
      FLAGS_iface << ":" << FLAGS_port << " is available" << std::endl;
	event_base_free(base);
	return -1;
  }

  google::protobuf::ShutdownProtobufLibrary();
  event_base_free(base);
  return 0;
}
