#ifndef _DANGER_STORAGE_H_
#define _DANGER_STORAGE_H_

#include <string>
#include <vector>
#include <leveldb/db.h>
#include <id3/tag.h>

namespace danger {

class Track {
  std::string m_path;
  std::string m_name;
  char *m_album;
  char *m_artist;
  char *m_title;
  char *m_year;
public:
  Track(std::string, std::string);
  Track(std::string, std::string, const ID3_Tag &);
  ~Track(void);
  std::string* path(void);
  std::string* name(void);
  std::string* artist(void);
};

class Storage {
  std::string m_musicpath;
  leveldb::DB* m_db;
  std::vector<Track*> m_library;
public:
  Storage(std::string musicdir, std::string dbpath);
  void update(void);
  const std::vector<Track*>* get_tracks(void);
  const std::string get_musicpath(void);
  ~Storage();
};

}
#endif
