#include <cassert>
#include <leveldb/db.h>
#include <leveldb/options.h>

#include <id3/tag.h>

#include <boost/filesystem.hpp>
#include <boost/regex.hpp>

#include <glog/logging.h>
#include <gflags/gflags.h>

#include "storage.h"
#include "protogen/track.pb.h"

DEFINE_bool(id3, true, "parse id3 data");

namespace danger {

Storage::Storage(std::string music, std::string dbpath) {
  m_musicpath = music;
  if (!m_musicpath.at(m_musicpath.length() - 1) != '/') {
    m_musicpath.append("/");
  }
  leveldb::Options opts;
  opts.create_if_missing = true;
  leveldb::Status status = leveldb::DB::Open(opts, dbpath, &m_db);
  assert(status.ok());
}

Storage::~Storage()
{
  delete m_db;
}

static bool
compare_tracks(Track *a, Track *b)
{
  const std::string *aname = a->name();
  const std::string *bname = b->name();
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
        Track *t;
        if (FLAGS_id3) {
          ID3_Tag myTag(name.c_str());
          t = new Track(m_musicpath, name, myTag);
        } else {
          t = new Track(m_musicpath, name);
        }
        t->write_to_level(m_db);
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

static char *
id3_get_string(const ID3_Frame *frame, ID3_FieldID fldName)
{
  char *text = NULL;
  if (NULL != frame)
    {
      ID3_Field* fld = frame->GetField(fldName);
      ID3_TextEnc enc = fld->GetEncoding();
      fld->SetEncoding(ID3TE_ASCII);
      size_t nText = fld->Size();
      text = new char[nText + 1];
      fld->Get(text, nText + 1);
      fld->SetEncoding(enc);
    }
  return text;
}

Track::Track(std::string prefix, std::string path, const ID3_Tag &tag)
{
  m_path = path;
  m_name = path.substr(prefix.length(), path.npos);
  m_album = NULL;
  m_artist = NULL;
  m_title = NULL;
  m_year = NULL;

  ID3_Frame *frame = NULL;
  if ((frame = tag.Find(ID3FID_LEADARTIST)) ||
      (frame = tag.Find(ID3FID_BAND))       ||
      (frame = tag.Find(ID3FID_CONDUCTOR))  ||
      (frame = tag.Find(ID3FID_COMPOSER))) {
    m_artist = id3_get_string(frame, ID3FN_TEXT);
  }

  if ((frame = tag.Find(ID3FID_ALBUM))) {
    m_album = id3_get_string(frame, ID3FN_TEXT);
  }
  if ((frame = tag.Find(ID3FID_TITLE))) {
    m_title = id3_get_string(frame, ID3FN_TEXT);
  }
  if ((frame = tag.Find(ID3FID_YEAR))) {
    m_year = id3_get_string(frame, ID3FN_TEXT);
  }
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

bool
Track::write_to_level(leveldb::DB *db)
{
  TrackP tp;
  tp.set_path(m_path);
  tp.set_name(m_name);
  if (m_album)
    tp.set_album(m_album);
  if (m_artist)
    tp.set_artist(m_artist);
  if (m_title)
    tp.set_title(m_title);
  if (m_year)
    tp.set_year(m_year);

  std::string val;
  tp.SerializeToString(&val);
  
  std::string key = "track::";
  key.append(m_name);

  leveldb::Status s;
  s = db->Put(leveldb::WriteOptions(), key, val);
  return s.ok();
}

Track::~Track(void)
{
  if (m_album)
    delete m_album;
  if (m_artist)
    delete m_artist;
  if (m_title)
    delete m_title;
  if (m_year)
    delete m_year;
}

}
