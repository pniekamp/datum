//
// util - collection of small useful routines
//
// Copyright (C) Peter Niekamp
//
// This code remains the property of the copyright holder.
// The code contained herein may be used for any purpose provided
// this copyright notice is retained
//

#ifndef LEAPUTIL_HH
#define LEAPUTIL_HH

#include <cstdlib>
#include <ostream>
#include <sstream>
#include <string>
#include <cstring>
#include <vector>
#include <limits>
#include <cmath>
#include <iterator>
#include <iomanip>
#include <memory>
#include <cctype>

/**
 * \namespace leap
 * \brief Leap Library containing common helper routines
 *
**/

/**
 * \defgroup leaputil Leap Utility Helpers
**/

namespace leap
{

  //|//////////// dereference ///////////////////////////////////////////////
  /**
   * \brief dereference arbitary pointer or reference
   * \ingroup leaputil
  **/

  template<typename T>
  struct dereference_impl
  {
    T &operator()(T &value) const { return value; }
    T &operator()(T *value) const { return *value; }
  };

  template<typename T>
  auto &dereference(T &value)
  {
    return dereference_impl<typename std::remove_pointer<T>::type>()(value);
  }


  //|//////////// fixedprecision ////////////////////////////////////////////
  /**
   * \brief Set iostream presicion to fixed without trailing zeros
   * \ingroup leaputil
  **/

  template<typename T>
  void setfixedprecision(std::ostream &/*os*/, T const &/*value*/, int /*precision*/)
  {
  }

  template<>
  inline void setfixedprecision(std::ostream &os, float const &value, int precision)
  {
    os.precision((int)std::log10(std::fabs(value)) + precision + 1);
  }

  template<>
  inline void setfixedprecision(std::ostream &os, double const &value, int precision)
  {
    os.precision((int)std::log10(std::fabs(value)) + precision + 1);
  }


  //|//////////// toa ///////////////////////////////////////////////////////
  /**
   * \brief Convert data type to string
   * \ingroup leaputil
   *
   * uses basic stringstream functions to convert data into string representation
   *
   * Example Usage :
   * \code
   *   #include <leap/util.h>
   *
   *   using namespace leap;
   *
   *   void Test()
   *   {
   *     string a = toa(54.3);
   *   }
   * \endcode
  **/

  template<typename T>
  std::string toa(T const &value, int precision = -1)
  {
    std::stringstream ss;

    if (precision != -1)
      setfixedprecision(ss, value, precision);

    ss << value;

    return ss.str();
  }

  template<>
  inline std::string toa(std::string const &value, int /*precision*/)
  {
    return value;
  }


  //|//////////// ato ///////////////////////////////////////////////////////
  /**
   * \brief Convert string to data type
   * \ingroup leaputil
   *
   * uses basic stringstream functions to convert an ascii string to data
   *
   * Example Usage :
   * \code
   *   #include <leap/util.h>
   *
   *   using namespace leap;
   *
   *   double Test()
   *   {
   *     return ato<double>("54.3");
   *   }
   * \endcode
  **/

  template<typename T>
  T ato(std::string const &str, T const &defaultvalue = T())
  {
    T result = defaultvalue;

    if (!str.empty())
    {
      std::stringstream ss(str);

      ss >> result;
    }

    return result;
  }

  template<>
  inline int ato(std::string const &str, int const &defaultvalue)
  {
    return (!str.empty()) ? atoi(str.c_str()) : defaultvalue;
  }

  template<>
  inline double ato(std::string const &str, double const &defaultvalue)
  {
    return (!str.empty()) ? atof(str.c_str()) : defaultvalue;
  }

  template<>
  inline std::string ato(std::string const &str, std::string const &defaultvalue)
  {
    return (!str.empty()) ? str : defaultvalue;
  }


  //|//////////// atov //////////////////////////////////////////////////////
  /**
   * \brief Convert string data type to vector
   * \ingroup leaputil
   *
   * uses basic stringstream functions to convert a string to vector data
   *
   * Example Usage :
   * \code
   *   #include <leap/util.h>
   *
   *   using namespace leap;
   *
   *   void Test()
   *   {
   *     vector<double> a = atov<double>("54.3 12.0");
   *   }
   * \endcode
  **/

  template<typename T>
  std::vector<T> atov(std::string const &str, const char *delimiters = ", \t")
  {
    std::stringstream ss(str);

    T v;
    std::vector<T> values;

    while (ss >> v)
    {
      values.push_back(v);

      while (std::strchr(delimiters, ss.peek()))
        ss.ignore(1);
    }

    return values;
  }

#ifdef __MINGW32__
  // mingw uses msvcrt which on XPSP3 has iostream thread safety bug
  template<>
  inline std::vector<double> atov(std::string const &str, const char *delimiters)
  {
    std::vector<double> result;

    size_t i = str.find_first_not_of(delimiters, 0);
    size_t j = str.find_first_of(delimiters, i);

    while (i != std::string::npos)
    {
      result.push_back(ato<double>(str.substr(i, j-i)));

      i = str.find_first_not_of(delimiters, j);
      j = str.find_first_of(delimiters, i);
    }

    return result;
  }
#endif


  //|//////////// vtoa //////////////////////////////////////////////////////
  /**
   * \brief Convert string to data type
   * \ingroup leaputil
   *
   * uses basic stringstream functions to convert a vector to an ascii string
   *
   * Example Usage :
   * \code
   *   #include <leap/util.h>
   *
   *   using namespace leap;
   *
   *   void Test(vector<double> const &v)
   *   {
   *     string a = vtoa(v);
   *   }
   * \endcode
  **/

  template<typename T>
  std::string vtoa(std::vector<T> const &v, int precision = -1)
  {
    std::ostringstream os;

    if (precision != -1)
    {
      for(auto i = v.begin(); i != v.end(); ++i)
      {
        setfixedprecision(os, *i, precision);

        os << *i << " ";
      }
    }
    else
      std::copy(v.begin(), v.end(), std::ostream_iterator<T>(os, " "));

    return os.str();
  }

/*
  template<>
  inline std::string vto(std::vector<T> const &v, int precision)
  {
  }
*/


  //|//////////// tolower ///////////////////////////////////////////////////
  /**
   * \brief runs tolower on string
   * \ingroup leaputil
  **/

  inline std::string tolower(std::string const &str)
  {
    auto result = str;

    for(size_t i = 0; i < result.length(); ++i)
      result[i] = std::tolower(result[i]);

    return result;
  }


  //|//////////// toupper ///////////////////////////////////////////////////
  /**
   * \brief runs toupper on string
   * \ingroup leaputil
  **/

  inline std::string toupper(std::string const &str)
  {
    auto result = str;

    for(size_t i = 0; i < result.length(); ++i)
      result[i] = std::toupper(result[i]);

    return result;
  }


  //|//////////// trim //////////////////////////////////////////////////////
  /**
   * \brief removes leading and trailing characters
   * \ingroup leaputil
  **/

  inline std::string trim(std::string const &str, const char *characters = " \t\r\n")
  {
    auto i = str.find_first_not_of(characters);
    auto j = str.find_last_not_of(characters);

    if (i == std::string::npos || j == std::string::npos)
      return "";

    return str.substr(i, j-i+1);
  }


  //|//////////// split /////////////////////////////////////////////////////
  /**
   * \brief separates string into components
   * \ingroup leaputil
  **/

  inline std::vector<std::string> split(std::string const &str, const char *delimiters = " \t\r\n")
  {
    std::vector<std::string> result;

    auto i = str.find_first_not_of(delimiters, 0);
    auto j = str.find_first_of(delimiters, i);

    while (i != std::string::npos)
    {
      result.push_back(str.substr(i, j-i));

      i = str.find_first_not_of(delimiters, j);
      j = str.find_first_of(delimiters, i);
    }

    return result;
  }


  //|//////////// index_sequence ////////////////////////////////////////////
  /**
   * \brief compile time index sequence
   * \ingroup leaputil
  **/

  template<size_t... Indices> struct index_sequence
  {
    typedef size_t value_type;

    static constexpr size_t size() noexcept { return sizeof...(Indices); }
  };

  // make_index_sequence
  template<size_t i, size_t j, size_t stride, typename = void>
  struct make_index_sequence_impl
  {
    template<typename>
    struct detail;

    template<size_t... Indices>
    struct detail<index_sequence<Indices...>>
    {
      using type = index_sequence<i, Indices...>;
    };

    using type = typename detail<typename make_index_sequence_impl<i+stride, j, stride>::type>::type;
  };

  template<size_t i, size_t j, size_t stride>
  struct make_index_sequence_impl<i, j, stride, std::enable_if_t<!(i < j)>>
  {
    using type = index_sequence<>;
  };

  template<size_t i, size_t j, size_t stride = 1>
  using make_index_sequence = typename make_index_sequence_impl<i, j, stride>::type;

  // get
  template<size_t i, typename IndexSequence>
  struct get_index_sequence_impl
  {
    template<size_t, typename>
    struct detail;

    template<size_t head, size_t... rest>
    struct detail<0, index_sequence<head, rest...>>
    {
      static constexpr size_t value = head;
    };

    template<size_t n, size_t head, size_t... rest>
    struct detail<n, index_sequence<head, rest...>>
    {
      static constexpr size_t value = detail<n-1, index_sequence<rest...>>::value;
    };

    using type = detail<i, IndexSequence>;
  };


  template<size_t i, size_t... Indices>
  size_t get(index_sequence<Indices...>)
  {
    return get_index_sequence_impl<i, index_sequence<Indices...>>::type::value;
  }


  //|//////////// fold //////////////////////////////////////////////////////
  /**
   * \brief compile time fold over parameter pack
   * \ingroup leaputil
  **/

  struct fold_impl
  {
    template<typename Func, typename T>
    static constexpr auto foldl(T const &first)
    {
      return first;
    }

    template<typename Func, typename T>
    static constexpr auto foldl(T &&first, T &&second)
    {
      return Func()(std::forward<T>(first), std::forward<T>(second));
    }

    template<typename Func, typename T, typename... Args>
    static constexpr auto foldl(T &&first, T &&second, Args&&... args)
    {
      return foldl<Func>(Func()(first, second), std::forward<Args>(args)...);
    }
  };

  template<typename Func, typename... Args>
  constexpr auto foldl(Args&&... args)
  {
    return fold_impl::foldl<Func>(std::forward<Args>(args)...);
  }


  //|//////////// alignto ///////////////////////////////////////////////////
  /**
   * \brief returns the offset rounded up to alignment
   * \ingroup leaputil
  **/

  template<typename T>
  constexpr T alignto(T const &offset, T const &alignment)
  {
    return (offset + alignment - 1) & -alignment;
  }


  //|//////////// extentof //////////////////////////////////////////////////
  /**
   * \brief returns the size of a basic array
   * \ingroup leaputil
  **/

  template<typename T>
  constexpr size_t extentof(T const &)
  {
    return std::extent<T>::value;
  }


  //|//////////// gather ////////////////////////////////////////////////////
  /**
   * \brief constexpr gather from c array to std::array
   * \ingroup leaputil
  **/

  template<typename T, size_t... Indices>
  constexpr auto tie(T *data, index_sequence<Indices...>)
  {
    return std::tie(data[Indices]...);
  }

  template<typename T, size_t... Indices>
  constexpr auto gather(T const *data, index_sequence<Indices...>)
  {
    return std::array<T, sizeof...(Indices)>{ data[Indices]... };
  }


  //|//////////// fill //////////////////////////////////////////////////////
  /**
   * \brief constexpr fill of std::array
   * \ingroup leaputil
  **/

  template<typename T, size_t... Indices>
  static constexpr auto fill(T const &value, index_sequence<Indices...>)
  {
    return std::array<T, sizeof...(Indices)>{ ((void)Indices,value)... };
  }


  //|//////////// slice /////////////////////////////////////////////////////
  /**
   * \brief constexpr slice of c array to std::array
   * \ingroup leaputil
  **/

  template<size_t i, size_t j, typename T>
  constexpr std::array<T, j - i> slice(T const *data)
  {
    return gather(data, make_index_sequence<i, j>());
  }


  //|//////////// rowslice //////////////////////////////////////////////////
  /**
   * \brief extracts a row slice from a 2d array
   * \ingroup leaputil
  **/

  template<typename T, size_t size1, size_t size2>
  constexpr std::array<T, size2> rowslice(T const (&A)[size1][size2], size_t row)
  {
    return gather((T const *)A + row*size2, make_index_sequence<0, size2, 1>());
  }


  //|//////////// columnslice ///////////////////////////////////////////////
  /**
   * \brief extracts a column slice from a 2d array
   * \ingroup leaputil
  **/

  template<typename T, size_t size1, size_t size2>
  constexpr std::array<T, size1> columnslice(T const (&A)[size1][size2], size_t column)
  {
    return gather((T const *)A + column, make_index_sequence<0, size1*size2, size2>());
  }


  //|//////////// fcmp //////////////////////////////////////////////////////
  /**
   * \brief Approximate comparison of two floating point numbers
   * \ingroup leaputil
  **/

  template<typename T, std::enable_if_t<std::is_arithmetic<T>::value>* = nullptr>
  constexpr bool fcmp(T a, T b, T epsilon)
  {
    return std::abs(a - b) < epsilon;
  }

  template<typename T, std::enable_if_t<std::is_floating_point<T>::value>* = nullptr>
  constexpr bool fcmp(T a, T b)
  {
    return std::abs(a - b) < std::numeric_limits<T>::epsilon() * std::max(std::abs(a), T(1));
  }

  template<typename T, std::enable_if_t<std::is_integral<T>::value>* = nullptr>
  constexpr bool fcmp(T a, T b)
  {
    return a == b;
  }


  //|//////////// sign //////////////////////////////////////////////////////
  /**
   * \brief sign of argument, -1, 0, 1
   * \ingroup leaputil
  **/

  template<typename T, std::enable_if_t<std::is_arithmetic<T>::value>* = nullptr>
  constexpr T sign(T arg)
  {
    return (arg == 0) ? 0 : (arg < 0) ? -1 : 1;
  }


  //|//////////// clamp /////////////////////////////////////////////////////
  /**
   * \brief clamp a value within lower and upper
   * \ingroup leaputil
  **/

  template<typename T, std::enable_if_t<std::is_scalar<T>::value>* = nullptr>
  constexpr T clamp(T value, T lower, T upper)
  {
    return std::max(lower, std::min(value, upper));
  }


  //|//////////// lerp //////////////////////////////////////////////////////
  /**
   * \brief lerp within lower and upper
   * \ingroup leaputil
  **/

  template<typename T, std::enable_if_t<std::is_scalar<T>::value>* = nullptr>
  constexpr T lerp(T lower, T upper, T alpha)
  {
    return (1-alpha)*lower + alpha*upper;
  }


  //|//////////// remap /////////////////////////////////////////////////////
  /**
   * \brief remap range1 to range2
   * \ingroup leaputil
  **/

  template<typename T, std::enable_if_t<std::is_scalar<T>::value>* = nullptr>
  constexpr T remap(T value1, T lower1, T upper1, T lower2, T upper2)
  {
    return lerp(lower2, upper2, (value1 - lower1)/(upper1 - lower1));
  }


  //|//////////// fmod2 /////////////////////////////////////////////////////
  /**
   * \brief fmod with negative wrap around
   * \ingroup leaputil
  **/

  template<typename T, std::enable_if_t<std::is_floating_point<T>::value>* = nullptr>
  T fmod2(T numerator, T denominator)
  {
    if (numerator < 0)
      return std::fmod(numerator, denominator) + denominator;
    else
      return std::fmod(numerator, denominator);
  }


  //|//////////// stricmp ///////////////////////////////////////////////////
  /**
   * \brief Compares two strings in a case-insensitive manner
   * \ingroup leaputil
  **/

  template<typename T>
  int stricmp(const T *str1, const T *str2)
  {
    while (std::tolower(*str1) == std::tolower(*str2) && *str1 != 0)
    {
      ++str1;
      ++str2;
    }

    return (std::tolower(*str2) - std::tolower(*str1));
  }


  //|//////////// stricmp ///////////////////////////////////////////////////
  /**
   * \brief Compares two strings in a case-insensitive manner
   * \ingroup leaputil
  **/

  template<typename T, class traits>
  int stricmp(std::basic_string<T, traits> const &str1, std::basic_string<T, traits> const &str2)
  {
    return stricmp(str1.c_str(), str2.c_str());
  }


  //|//////////// strincmp //////////////////////////////////////////////////
  /**
   * \brief Compares two strings in a case-insensitive manner
   * \ingroup leaputil
  **/

  template<typename T>
  int strincmp(const T *str1, const T *str2, size_t n)
  {
    while (std::tolower(*str1) == std::tolower(*str2) && *str1 != 0 && n > 1)
    {
      ++str1;
      ++str2;
      --n;
    }

    return (std::tolower(*str2) - std::tolower(*str1));
  }


  //|//////////// strincmp //////////////////////////////////////////////////
  /**
   * \brief Compares two strings in a case-insensitive manner
   * \ingroup leaputil
  **/

  template<typename T, class traits>
  int strincmp(std::basic_string<T, traits> const &str1, std::basic_string<T, traits> const &str2, size_t n)
  {
    return strincmp(str1.c_str(), str2.c_str(), n);
  }


  //|//////////// strxpnd ///////////////////////////////////////////////////
  /**
   * \brief Expands (Escaped) Control Characters
   * \ingroup leaputil
  **/

  template<typename T, class traits>
  std::basic_string<T, traits> strxpnd(std::basic_string<T, traits> const &src)
  {
    std::basic_string<T, traits> result;

    size_t pos = 0;
    size_t index = 0;

    while ((index = src.find('\\', pos)) != std::basic_string<T, traits>::npos)
    {
      result += src.substr(pos, index-pos);

      if (index+1 >= src.size())
      {
        pos = index;
        break;
      }

      if (src[index+1] == '\\')
        result += '\\';

      else if (src[index+1] == 'n')
        result += '\n';

      else if (isdigit(src[index+1]))
      {
        int scancode = (src[index+1] - '0');

        while (isdigit(src[index+2]))
        {
          scancode = (scancode * 10) + (src[index+2] - '0');
          ++index;
        }

        result += scancode;
      }
      else
        result += std::tolower(src[index+1]) - 'a' + 1;

      pos = index+2;
    }

    result += src.substr(pos);

    return result;
  }


  template<typename T>
  std::basic_string<T> strxpnd(const T *src)
  {
    return strxpnd(std::basic_string<T>(src));
  }


  //|//////////// setenv ////////////////////////////////////////////////////
  /**
   * \brief Set Environment Variable
   * \ingroup leaputil
  **/

  template<typename T, class traits>
  void setenv(std::basic_string<T, traits> const &envname, std::basic_string<T, traits> const &envval)
  {
    auto envstr = envname + "=" + envval;

    char *env = new char[envstr.length()+1];

    for(size_t i = 0; i < envstr.length(); ++i)
      env[i] = envstr[i];

    env[envstr.length()] = 0;

    putenv(env);
  }


  template<typename T>
  void setenv(const T *envname, const T *envval)
  {
    setenv(std::basic_string<T>(envname), std::basic_string<T>(envval));
  }



  //|//////////// strvpnd ///////////////////////////////////////////////////
  /**
   * \brief Expands Environment Variables
   * \ingroup leaputil
  **/

  template<typename T, class traits>
  std::basic_string<T, traits> strvpnd(std::basic_string<T, traits> const &src)
  {
    std::basic_string<T, traits> result;

    size_t pos = 0;
    size_t next = 0;

    while ((next = src.find('$', next)) != std::basic_string<T, traits>::npos)
    {
      size_t beg = src.find('{', next+1);
      size_t end = src.find('}', next+1);

      if (beg == next+1 && end != std::basic_string<T, traits>::npos)
      {
        result += src.substr(pos, next-pos);

        // Extract Variable Name (in non wide characters)
        char variable[128];
        for(size_t i = 0; i < end-beg-1; ++i)
          variable[i] = src[beg+1+i];
        variable[end-beg-1] = 0;

        // Get Environment Variable
        const char *env = getenv(variable);

        if (env != NULL)
        {
          // Copy in variable text
          while (*env != 0)
          {
            if (*env == '\\')
              result += '\\';   // Escape slashes

            result += *env++;
          }
       }

        pos = end+1;
        next = end;
      }

      ++next;
    }

    result += src.substr(pos);

    return result;
  }


  template<typename T>
  std::basic_string<T> strvpnd(const T *src)
  {
    return strvpnd(std::basic_string<T>(src));
  }

} // namespace

#endif
