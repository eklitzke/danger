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
  const std::string &aname = a->name();
  const std::string &bname = b->name();
  if (aname.compare(bname) == -1)
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
        LOG(INFO) << "found track: " << iter->path();

        std::string fname = name.substr(m_musicpath.length(), name.npos);
        Track *t = new Track(name, fname);

        std::string value;
        std::string keyname = "track::";
        keyname.append(t->name());
        if (!t->parse_from_level(m_db)) {
          if (FLAGS_id3) {
            t->parse_id3();
          }
          t->write_to_level(m_db);
        }
        m_library.push_back(t);
      }
    }
  std::sort(m_library.begin(), m_library.end(), compare_tracks);
  LOG(INFO) << "done updating";
}

void
Storage::update_from_level(void)
{
  leveldb::Iterator* it = m_db->NewIterator(leveldb::ReadOptions());
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    if (strncmp(it->key().ToString().c_str(), "track::", 7) == 0) {
      std::string val = it->value().ToString();
      val = val.substr(7, val.npos);
      std::string x = m_musicpath;
      x.append(val);

      Track *t = new Track(x, val);
      t->parse_from_level(m_db); // XXX: this does an extra/unnecessary db fetch
      m_library.push_back(t);
    }
  }
  assert(it->status().ok());  // Check for any errors found during the scan
  delete it;
  LOG(INFO) << "done updating, found " << m_library.size() << " tracks";
}

std::string
Storage::get_musicpath(void) const
{
  return m_musicpath;
}

const std::vector<Track*>*
Storage::get_tracks(void) const
{
  return &m_library;
}

static std::string const
id3_get_string(const ID3_Frame *frame, ID3_FieldID fldName)
{
  char *text = NULL;
  if (frame != NULL) {
    ID3_Field* fld = frame->GetField(fldName);
    ID3_TextEnc enc = fld->GetEncoding();
    fld->SetEncoding(ID3TE_ASCII);
    size_t nText = fld->Size();
    text = new char[nText + 1];
    fld->SetEncoding(enc);
    fld->Get(text, nText + 1);
  }
  std::string t = std::string(text);
  delete text;
  return t;
}

Track::Track(std::string path, std::string name)
{
  m_path = path;
  m_name = name;
}

void
Track::parse_id3(void)
{
  ID3_Tag tag(m_path.c_str());
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

const std::string &
Track::name(void)
{
  return m_name;
}

bool
Track::write_to_level(leveldb::DB *db)
{
  TrackP tp;
  tp.set_path(m_path);
  tp.set_name(m_name);
  if (m_album.length())
    tp.set_album(m_album);
  if (m_artist.length())
    tp.set_artist(m_artist);
  if (m_title.length())
    tp.set_title(m_title);
  if (m_year.length())
    tp.set_year(m_year);

  std::string val;
  tp.SerializeToString(&val);
  
  std::string key = "track::";
  key.append(m_name);

  leveldb::Status s;
  s = db->Put(leveldb::WriteOptions(), key, val);
  return s.ok();
}

bool
Track::parse_from_level(leveldb::DB *db)
{
  std::string value;
  std::string keyname = "track::";
  keyname.append(m_name);

  leveldb::Status s = db->Get(leveldb::ReadOptions(), keyname, &value);
  if (s.ok()) {
    TrackP p;
    p.ParseFromString(value);
    m_album = p.album();
    m_artist = p.artist();
    m_title = p.title();
    m_year = p.year();
    return true;
  }
  return false;
}

}
