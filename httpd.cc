#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <event2/buffer.h>
#include <event2/event.h>
#include <event2/http.h>

#include <glog/logging.h>

#include <ctemplate/template.h>  

#include <yajl/yajl_gen.h>  

#include <iostream>
#include <map>
#include <string>

#include <gflags/gflags.h>

#include "storage.h"

DEFINE_int32(port, 9000, "the port to listen on");
DEFINE_string(iface, "0.0.0.0", "the interface to bind to");

#define BUFSIZE 8000

#define UNSIGNED_STRING  (const unsigned char *)

namespace danger {

  std::map<std::string, std::string> content_types;
  Storage *storage = NULL;
  struct evhttp *server = NULL;

  void
  add_headers(struct evhttp_request *req, const std::string &content_type)
  {
    struct evkeyvalq *headers;
    headers = evhttp_request_get_output_headers(req);
    evhttp_add_header(headers, "Server", "danger-httpd/0.1");
    if (content_type.length()) {
      evhttp_add_header(headers, "Content-Type", content_type.c_str());
    }
  }

  void
  respond_template(struct evhttp_request *req, const ctemplate::TemplateDictionary &dict, int status, const char *reason)
  {
    std::string output;
    struct evbuffer *response;

    add_headers(req, "text/html; charset=utf-8");

    ctemplate::ExpandTemplate("templates/base.html", ctemplate::STRIP_BLANK_LINES, &dict, &output);
    response = evbuffer_new();
    if (response != NULL) {
      evbuffer_add(response, output.c_str(), output.length());
      evhttp_send_reply(req, status, reason, response);
      evbuffer_free(response);
    } else {
      std::cerr << "failed to evbuffer_new()" << std::endl;
    }
    assert(!evhttp_request_is_owned(req));
  }

  void
  req_notfound(struct evhttp_request *req)
  {
    ctemplate::TemplateDictionary dict("not_found");
    dict.SetValue("TITLE", "document not found");
    ctemplate::TemplateDictionary *child_dict = dict.AddIncludeDictionary("BODY");
    child_dict->SetFilename("templates/error.html");
    respond_template(req, dict, HTTP_NOTFOUND, "Not Found");
  }
  
  void
  req_static_file(struct evhttp_request *req, const std::string &path_to_file)
  {
    int fd = open(path_to_file.c_str(), O_RDONLY);
    if (fd < 0) {
      req_notfound(req);
      return;
    }
    struct stat buf;
    if (fstat(fd, &buf) != 0) {
      close(fd);
      req_notfound(req);
      return;
    }

    size_t dotpos = path_to_file.find_last_of('.');
    if (dotpos == path_to_file.npos) {
      add_headers(req, "application/octet-stream");
    } else {
      std::string extension = path_to_file.substr(dotpos + 1, path_to_file.length() - dotpos);
      if (content_types.count(extension)) {
        add_headers(req, content_types[extension].c_str());
      } else {
        add_headers(req, "application/octet-stream");
      }
    }

    struct evbuffer *response = evbuffer_new();
#ifdef NO_EVBUFFER_ADD_FILE
    // FIXME: we're supposed to be bale to evbuffer_add_file() here, which uses
    // sendfile(2) and is more efficient. That seems to sometimes cause problems
    // with SIGPIPE though (for unknown reasons), so do this the slow way.
    while (true) {
      static char buf[BUFSIZE];
      ssize_t bytes_read = read(fd, buf, BUFSIZE);
      assert(bytes_read >= 0);
      if (bytes_read == 0)
        break;
      evbuffer_add(response, buf, bytes_read);
    }
    close(fd);
#else
    // this will close the fd itself
    evbuffer_add_file(response, fd, 0, buf.st_size);
#endif
    evhttp_send_reply(req, HTTP_OK, "OK", response);
    return;
  }

  static void
  req_generic_cb(struct evhttp_request *req, void *data)
  {
    const struct evhttp_uri *evuri = evhttp_request_get_evhttp_uri(req);
    const char *uripath = evhttp_uri_get_path(evuri);
    LOG(INFO) << "got request to " << uripath;
    size_t pathsize;
    char *path = evhttp_uridecode(uripath, 0, &pathsize);
    if (strncmp(path, "/fetch/", 7) == 0) {
      size_t offset = 7;
      while (offset < pathsize && *(path + offset) == '/') {
        offset++;
      }
      std::string path_to_file = storage->get_musicpath();
      path_to_file.append(path + offset);
      free(path);
      req_static_file(req, path_to_file);
    } else if (strncmp(path, "/static/", 8) == 0) {
      size_t offset = 8;
      while (offset < pathsize && *(path + offset) == '/') {
        offset++;
      }
      std::string static_path = "./static/";
      static_path.append(path + offset);
      free(path);
      req_static_file(req, static_path);
    } else {
      free(path);
      req_notfound(req);
    }
  }

  void
  req_home(struct evhttp_request *req, void *data)
  {
    ctemplate::TemplateDictionary dict("home");
    dict.SetValue("TITLE", "danger!");
    ctemplate::TemplateDictionary *child_dict = dict.AddIncludeDictionary("BODY");
    child_dict->SetFilename("templates/home.html");
    respond_template(req, dict, HTTP_OK, "OK");
  }

  static void
  add_map_pair_s(const yajl_gen &g, const char *name, size_t slen, const std::string &val)
  {
    yajl_gen_string(g, UNSIGNED_STRING name, slen);
    yajl_gen_string(g, UNSIGNED_STRING val.c_str(), val.length());
  }

  static void
  add_map_pair_i(const yajl_gen &g, const char *name, size_t slen, int val)
  {
    yajl_gen_string(g, UNSIGNED_STRING name, slen);
    yajl_gen_integer(g, val);
  }


  void
  req_list_library(struct evhttp_request *req, void *data)
  {
    LOG(INFO) << "serving library";
    const unsigned char *buf;  
    unsigned int buflen;  

    // FIXME: it looks like there's a way to re-use the yajl_gen object, using
    // yajl_gen_clear, but that wasn't working so I'm recreating it each
    // time. That would be slightly faster, though.
    yajl_gen_config conf = {1, " "};
    yajl_gen gen = yajl_gen_alloc(&conf, NULL);

    yajl_gen_array_open(gen);
    
    const std::vector<Track*> *tracks = storage->get_tracks();
    for (std::vector<Track *>::const_iterator it = tracks->begin(); it != tracks->end(); ++it) {
      Track *t = *it;
      yajl_gen_map_open(gen);
      add_map_pair_s(gen, "album", 5, t->album());
      add_map_pair_s(gen, "artist", 6, t->artist());
      add_map_pair_s(gen, "name", 4, t->name());
      add_map_pair_s(gen, "title", 5, t->title());
      add_map_pair_i(gen, "tracknum", 8, t->tracknum());
      add_map_pair_i(gen, "year", 4, t->year());
      yajl_gen_map_close(gen);
    }
    yajl_gen_array_close(gen);
    yajl_gen_get_buf(gen, &buf, &buflen); 

    // FIXME: this is a major hack
    char *b = (char *) malloc(buflen);
    memcpy(b, (const void *) buf, buflen);
    b[0] = '[';

    add_headers(req, "application/json");

    struct evbuffer *response = evbuffer_new();
    evbuffer_add(response, b, buflen);
    evhttp_send_reply(req, HTTP_OK, "OK", response);
    free(b);
    yajl_gen_free(gen);
    LOG(INFO) << "done generating library";

  }

  bool
  initialize_httpd(struct event_base *base, Storage *s)
  {

    storage = s;
    content_types["css"] = "text/css";
    content_types["js"]  = "text/javascript";
    content_types["mp3"] = "audio/mpeg";
    content_types["mp4"] = "audio/mp4";
    content_types["m4a"] = "audio/mp4";
    content_types["oga"] = "audio/ogg";
    content_types["ogg"] = "audio/ogg";
    
    if ((server = evhttp_new(base)) == NULL) {
      return false;
    }
    evhttp_set_allowed_methods(server,  EVHTTP_REQ_GET);
    evhttp_set_cb(server, "/", req_home, NULL);
    evhttp_set_cb(server, "/library", req_list_library, NULL);
    evhttp_set_gencb(server, req_generic_cb, NULL);
    evhttp_bind_socket(server, FLAGS_iface.c_str(), FLAGS_port);
    return true;
  }


}
