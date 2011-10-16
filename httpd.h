#ifndef _DANGER_HTTPD_H_
#define _DANGER_HTTPD_H_

#include <string>
#include <gflags/gflags.h>

#include "storage.h"

namespace danger {
  struct event_base *base;
  bool initialize_httpd(struct event_base *, Storage *);
}
	
#endif
