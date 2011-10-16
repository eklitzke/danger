#include <cassert>
#include <leveldb/db.h>

#include <id3/tag.h>

#include <boost/filesystem.hpp>
#include <boost/regex.hpp>

#include <glog/logging.h>
#include <gflags/gflags.h>

#include "storage.h"

DEFINE_bool(id3, true, "parse id3 data");

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
  ID3_Tag myTag;

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
          myTag.Link(name.c_str());
          t = new Track(m_musicpath, name, myTag);
        } else {
          t = new Track(m_musicpath, name);
        }

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

  std::cout << "----------" << std::endl;
  ID3_Frame *frame = NULL;
  if ((frame = tag.Find(ID3FID_LEADARTIST)) ||
      (frame = tag.Find(ID3FID_BAND))       ||
      (frame = tag.Find(ID3FID_CONDUCTOR))  ||
      (frame = tag.Find(ID3FID_COMPOSER))) {
    m_artist = id3_get_string(frame, ID3FN_TEXT);
    std::cout << m_artist << std::endl;
  }

  if ((frame = tag.Find(ID3FID_ALBUM))) {
    m_album = id3_get_string(frame, ID3FN_TEXT);
    std::cout << m_album << std::endl;
  }
  if ((frame = tag.Find(ID3FID_TITLE))) {
    m_title = id3_get_string(frame, ID3FN_TEXT);
    std::cout << m_title << std::endl;
  }
  if ((frame = tag.Find(ID3FID_YEAR))) {
    m_year = id3_get_string(frame, ID3FN_TEXT);
    std::cout << m_year << std::endl;
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
