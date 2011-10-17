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
  std::string m_album;
  std::string m_artist;
  std::string m_title;
  std::string m_year;
public:
  Track(std::string, std::string);
  ~Track(void);
  void parse_id3(void);
  std::string* path(void);
  const std::string& name(void);
  std::string* artist(void);
  bool write_to_level(leveldb::DB *);
  bool parse_from_level(leveldb::DB *);
};

class Storage {
  std::string m_musicpath;
  leveldb::DB* m_db;
  std::vector<Track*> m_library;
public:
  Storage(std::string musicdir, std::string dbpath);
  void update(void);
  void update_from_level(void);
  const std::vector<Track*>* get_tracks(void) const;
  std::string get_musicpath(void) const;
  ~Storage();
};

}
#endif
