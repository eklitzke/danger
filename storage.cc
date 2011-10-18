#include <cassert>
#include <string>

#include <unicode/ucnv.h>
#include <unicode/ucsdet.h>

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

static void
convert_to_utf8(UCharsetDetector *csd, std::string &source)
{
  const UCharsetMatch *ucm;
  UErrorCode status = U_ZERO_ERROR;
  ucsdet_setText(csd, source.c_str(), source.length(), &status);
  assert(status == U_ZERO_ERROR);
  ucm = ucsdet_detect(csd, &status);
  assert(status == U_ZERO_ERROR);

  // FIXME: on some obscure filenames this crashes with a SEGFAULT, sort of like
  // this:
  //    Program received signal SIGSEGV, Segmentation fault.
  //  icu_46::CharsetMatch::getName (this=0x0) at csmatch.cpp:36
  //  36	    return csr->getName(); 
  //  (gdb) bt
  //  #0  icu_46::CharsetMatch::getName (this=0x0) at csmatch.cpp:36
  //  #1  0x000000000040dadc in danger::convert_to_utf8 (csd=0x64b420, source="\246\246\246") at storage.cc:163
  //  #2  0x000000000040dfc7 in danger::Track::parse_id3 (this=0xa95040, csd=0x64b420) at storage.cc:199
  //  #3  0x000000000040eb3e in danger::Storage::update (this=0x7fffffffe088) at storage.cc:83
  //  #4  0x000000000040c813 in main (argc=1, argv=0x7fffffffe1d8) at main.cc:28
  const char *cs_name = ucsdet_getName(ucm, &status);
  assert(status == U_ZERO_ERROR);

  size_t out_len = source.length() * 16;
  char out_buf[out_len];
  if (strcmp(cs_name, "UTF-8") != 0) {
    // FIXME: i believe it's faster to pre-allocate the converters
    ucnv_convert("UTF-8", cs_name, out_buf, out_len, source.c_str(), source.length(), &status);
    if (status != U_ZERO_ERROR) {
      LOG(WARNING) << "skipping " << source;
      source = "";
    } else {
      source = out_buf;
    }
  }
}

Storage::Storage(std::string music, std::string dbpath) {
  m_musicpath = music;
  if (!m_musicpath.at(m_musicpath.length() - 1) != '/') {
    m_musicpath.append("/");
  }
  UErrorCode icu_status = U_ZERO_ERROR;
  m_csd = ucsdet_open(&icu_status);
  assert(icu_status == U_ZERO_ERROR);

  leveldb::Options opts;
  opts.create_if_missing = true;
  leveldb::Status l_status = leveldb::DB::Open(opts, dbpath, &m_db);
  assert(l_status.ok());
}

Storage::~Storage()
{
  ucsdet_close(m_csd);
  std::vector<Track *>::iterator it;
  for (it = m_library.begin(); it < m_library.end(); it++) {
    delete *it;
  }
  delete m_db;
}

#define CMPARE_S(thing)                      \
  const std::string &a_##thing = a->thing(); \
  const std::string &b_##thing = b->thing(); \
  cmp = a_##thing.compare(b_##thing); \
  if (cmp < 0) \
    return true; \
  else if (cmp > 0) \
    return false;

#define CMPARE_I(thing) \
  const int &a_##thing = a->thing(); \
  const int &b_##thing = b->thing(); \
  return a_##thing < b_##thing;

static bool
compare_tracks(Track *a, Track *b)
{
  int cmp;

  CMPARE_S(artist)
  CMPARE_S(album)
  CMPARE_I(tracknum)
  CMPARE_I(year)
  CMPARE_S(name)
  //assert(false);
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
      // no need to convert this to UTF-8
      std::string name = iter->path().generic_string();

      if (regex_match(name, pattern)) {
        LOG(INFO) << "found track: " << iter->path();

        std::string fname = name.substr(m_musicpath.length(), name.npos);
        convert_to_utf8(m_csd, fname);
        Track *t = new Track(name, fname);

        std::string value;
        std::string keyname = "track::";
        keyname.append(t->name());
        if (!t->parse_from_level(m_db)) {
          if (FLAGS_id3) {
            t->parse_id3(m_csd);
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
    if (it->key().ToString().compare(0, 7, "track::") == 0) {
      std::string val = it->key().ToString();
      val = val.substr(7, std::string::npos);
      std::string x = m_musicpath;
      x.append(val);

      Track *t = new Track(x, val);
      t->parse_from_level(m_db); // XXX: this does an extra/unnecessary db fetch
      m_library.push_back(t);
    }
  }
  assert(it->status().ok());  // Check for any errors found during the scan
  delete it;
  std::sort(m_library.begin(), m_library.end(), compare_tracks);
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

Track::~Track(void) { }


void
Track::parse_id3(UCharsetDetector *csd)
{
  ID3_Tag tag(m_path.c_str());
  ID3_Frame *frame = NULL;
  if ((frame = tag.Find(ID3FID_LEADARTIST)) ||
      (frame = tag.Find(ID3FID_BAND))       ||
      (frame = tag.Find(ID3FID_CONDUCTOR))  ||
      (frame = tag.Find(ID3FID_COMPOSER))) {
    m_artist = id3_get_string(frame, ID3FN_TEXT);
    convert_to_utf8(csd, m_artist);
  }

  if ((frame = tag.Find(ID3FID_ALBUM))) {
    m_album = id3_get_string(frame, ID3FN_TEXT);
    convert_to_utf8(csd, m_album);
  }
  if ((frame = tag.Find(ID3FID_TITLE))) {
    m_title = id3_get_string(frame, ID3FN_TEXT);
    convert_to_utf8(csd, m_title);
  }
  if ((frame = tag.Find(ID3FID_YEAR))) {
    std::string t = id3_get_string(frame, ID3FN_TEXT);
    convert_to_utf8(csd, t);
    m_year = atoi(t.c_str());
  }
  if ((frame = tag.Find(ID3FID_TRACKNUM))) {
    std::string t = id3_get_string(frame, ID3FN_TEXT);
    convert_to_utf8(csd, t);
    size_t first_slash = t.find_first_of('/');
    if (first_slash != std::string::npos) {
      t = t.substr(0, first_slash);
    }
    m_tracknum = atoi(t.c_str());
  }

}

std::string *
Track::path(void)
{
  return &m_path;
}

std::string const &
Track::name(void) const
{
  return m_name;
}

std::string const &
Track::album(void) const
{
  return m_album;
}

std::string const &
Track::artist(void) const
{
  return m_artist;
}

std::string const &
Track::title(void) const
{
  return m_title;
}

int const &
Track::tracknum(void) const
{
  return m_tracknum;
}

int const &
Track::year(void) const
{
  return m_year;
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
  if (m_tracknum != 0)
    tp.set_tracknum(m_tracknum);
  if (m_year != 0)
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
    m_tracknum = p.tracknum();
    m_year = p.year();
    return true;
  } else {
    LOG(WARNING) << "failed to find " << keyname;
  }
  return false;
}

}
