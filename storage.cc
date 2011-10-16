#include <cassert>
#include <leveldb/db.h>

#include <boost/filesystem.hpp>
#include <boost/regex.hpp>

#include <glog/logging.h>

#include "storage.h"

namespace danger {

Storage::Storage(std::string music, std::string dbpath) {
  m_musicpath = music;
  if (!m_musicpath.at(m_musicpath.length() - 1) != '/') {
    m_musicpath.append("/");
  }
  //library = new std::vector<Track>;
  //leveldb::Options options;
  //options.create_if_missing = true;
  //leveldb::Status status = leveldb::DB::Open(options, dbpath, &db);
  //assert(status.ok());
}

Storage::~Storage()
{
  //delete m_db;
}

static bool
compare_tracks(Track *a, Track *b)
{
  std::string *aname = a->name();
  std::string *bname = b->name();
  if (aname->compare(*bname) == -1)
    return true;
  else
    return false;
}

void
Storage::update(void)
{
  boost::filesystem::path music(m_musicpath);
  boost::regex pattern(".*\\.(mp3|mp4|m4a|oga|ogg)$");
  for (boost::filesystem::recursive_directory_iterator iter(music), end;
       iter != end;
       ++iter)
    {
      std::string name = iter->path().generic_string();
      if (regex_match(name, pattern)) {
        LOG(INFO) << "found new track: " << iter->path();
        Track *t = new Track(m_musicpath, name);
        m_library.push_back(t);
      }
    }
  std::sort(m_library.begin(), m_library.end(), compare_tracks);
  LOG(INFO) << "done updating";
}

const std::string
Storage::get_musicpath(void)
{
  return m_musicpath;
}

const std::vector<Track*>*
Storage::get_tracks(void) {
  return &m_library;
}

Track::Track(std::string prefix, std::string path)
{
  m_path = path;
  m_name = path.substr(prefix.length(), path.npos);
}

std::string *
Track::path(void)
{
  return &m_path;
}

std::string *
Track::name(void)
{
  return &m_name;
}

}
