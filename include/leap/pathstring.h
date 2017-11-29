//
// Path String Object
//
//   Peter Niekamp, January 2008
//

//
// Copyright (C) Peter Niekamp
//
// This code remains the property of the copyright holder.
// The code contained herein may be used for any purpose provided
// this copyright notice is retained
//

#ifndef PATHSTRING_HH
#define PATHSTRING_HH

#include <leap/stringview.h>

/**
 * \namespace leap
 * \brief Leap Library containing common helper routines
 *
**/

namespace leap
{

  //|------------------------ pathstring ------------------------------------
  //|------------------------------------------------------------------------
  /**
   * \brief PathString
   *
   * pathstring acts like a std::string with added Path Based Functionality.
   * This includes expansion to full path (relative to a base path) and
   * splitting into seperate path and filename strings.
   *
  **/

  class pathstring
  {
    public:
      pathstring() = default;
      pathstring(const char *path);
      pathstring(std::string const &path);
      explicit pathstring(string_view path);
      explicit pathstring(string_view base, string_view path);

      operator std::string const &() const { return m_path; }

      const char *c_str() const { return m_path.c_str(); }
      std::string const &path() const { return m_path; }

      std::string base() const;
      std::string name() const;
      std::string ext() const;

    private:

      void set(string_view base, string_view path);

      std::string m_path;
  };


  //|//////////////////// operator == ///////////////////////////////////////
  inline bool operator ==(pathstring const &lhs, pathstring const &rhs)
  {
    return (lhs.path() == rhs.path());
  }


  //|//////////////////// operator != ///////////////////////////////////////
  inline bool operator !=(pathstring const &lhs, pathstring const &rhs)
  {
    return !(lhs == rhs);
  }


  //|//////////////////// operator << ///////////////////////////////////////
  template<typename E, typename T>
  std::basic_ostream<E, T> &operator <<(std::basic_ostream<E, T> &os, pathstring const &str)
  {
    os << str.path();

    return os;
  }

/*

  //|------------------------ RetainCurrentDirectory ------------------------
  //|------------------------------------------------------------------------

  class RetainCurrentDirectory
  {
    public:
      RetainCurrentDirectory();
      ~RetainCurrentDirectory();

      void restore();

    private:

      char m_currentdirectory[512];
  };



  ///////////////////// Executeable BasePath ////////////////
  SmartPath GetExecutablePath();
*/

} // namespace

#endif // PATHSTRING_HH
