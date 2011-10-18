#ifndef _DANGER_STORAGE_H_
#define _DANGER_STORAGE_H_

#include <string>
#include <vector>
#include <leveldb/db.h>
#include <id3/tag.h>
#include <unicode/ucsdet.h>

namespace danger {

class Track {
  std::string m_path;
  std::string m_name;
  std::string m_album;
  std::string m_artist;
  std::string m_title;
  int m_tracknum;
  int m_year;

public:
  Track(std::string, std::string);
  ~Track(void);
  void parse_id3(UCharsetDetector *csd);
  std::string* path(void);
  std::string const & name(void) const;
  std::string const & album(void) const;
  std::string const & artist(void) const;
  std::string const & title(void) const;
  int const & tracknum(void) const;
  int const & year(void) const;
  bool write_to_level(leveldb::DB *);
  bool parse_from_level(leveldb::DB *);
};

class Storage {
  std::string m_musicpath;
  leveldb::DB* m_db;
  std::vector<Track*> m_library;
  UCharsetDetector *m_csd;

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
