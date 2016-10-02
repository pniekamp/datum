//
// pathstring.cpp implementation of a path string
//
//   Peter Niekamp, September, 2000
//

//
// Copyright (C) Peter Niekamp
//
// This code remains the property of the copyright holder.
// The code contained herein may be used for any purpose provided
// this copyright notice is retained
//

#include "leap/pathstring.h"
#include <cstring>
#include <cstdio>
#include <algorithm>

#ifdef _WIN32
#  include <windows.h>
#else
#  include <unistd.h>
#endif

using namespace std;
using namespace leap;


///////////// exeBasePath ///////////////
string exeBasePath()
{
  char path[FILENAME_MAX];

#ifdef _WIN32
  GetModuleFileName(NULL, path, sizeof(path));
#else
  if (readlink("/proc/self/exe", path, sizeof(path)) < 0)
    path[0] = 0;
#endif

  path[sizeof(path)-1] = 0;

#ifdef _WIN32
  replace(path, path+sizeof(path), '\\', '/');
#endif

  // strip exe name
  for(int i = strlen(path)-1; i > 0 && path[i] != '/'; --i)
    path[i] = 0;

  return path;
}


///////////// isRelative ////////////////
bool isRelative(string const &path)
{
  return !((path.size() == 0) || (path.size() > 0 && path[0] == '/') || (path.size() > 1 && path[1] == ':'));
}



//|------------------------- pathstring -------------------------------------
//|--------------------------------------------------------------------------


//|///////////////////// pathstring::Constructor ////////////////////////////
pathstring::pathstring(const char *path)
{
  set(exeBasePath(), path);
}


//|///////////////////// pathstring::Constructor ////////////////////////////
pathstring::pathstring(string const &path)
{
  set(exeBasePath(), path);
}


//|///////////////////// pathstring::Constructor ////////////////////////////
pathstring::pathstring(string const &base, string const &path)
{
  set(base, path);
}


//|///////////////////// pathstring::set ////////////////////////////////////
void pathstring::set(std::string const &base, std::string const &path)
{
  if (isRelative(path))
  {
    if (base.empty() || base[base.size()-1] == '/' || base[base.size()-1] == '\\')
      m_path = base + path;
    else
      m_path = base + "/" + path;
  }
  else
    m_path = path;

#ifdef _WIN32
  replace(m_path.begin(), m_path.end(), '\\', '/');
#endif
}


//|///////////////////// pathstring::base ///////////////////////////////////
string pathstring::base() const
{
  string base = m_path.substr(0, m_path.rfind('/'));

  if (base.find('/') == string::npos && m_path.find('/') != string::npos)
    base += '/';

  return base;
}


//|///////////////////// pathstring::name ///////////////////////////////////
string pathstring::name() const
{
  return m_path.substr(m_path.rfind('/') + 1);
}


//|///////////////////// pathstring::ext ////////////////////////////////////
string pathstring::ext() const
{
  return (m_path.find('.') != string::npos) ? m_path.substr(m_path.rfind('.') + 1) : "";
}
