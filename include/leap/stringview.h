//
// stringview
//
// Copyright (C) Peter Niekamp
//
// This code remains the property of the copyright holder.
// The code contained herein may be used for any purpose provided
// this copyright notice is retained
//

#ifndef LMLSTRINGVIEW_HH
#define LMLSTRINGVIEW_HH

#include <string>
#include <stdexcept>
#include <algorithm>

/**
 * \namespace leap
 * \brief Leap Leap Library containing common helper routines
 *
**/

/**
 * \defgroup leapdata Data Containors
 * \brief Data Containors
 *
**/

namespace leap
{

  //|-------------------- string_view ---------------------------------------
  //|------------------------------------------------------------------------
  /**
   * \ingroup ingroup leapdata
   *
   * \brief string_view helper
   *
  **/

  template<typename T, class traits = std::char_traits<T>>
  class basic_string_view
  {
    public:

      typedef T value_type;
      typedef T char_type;
      typedef traits traits_type;
      typedef T const *const_iterator;

    public:
      basic_string_view() = default;
      constexpr basic_string_view(T const *str);
      constexpr basic_string_view(T const *str, size_t len);
      constexpr basic_string_view(std::basic_string<T, traits> const &str);

      constexpr bool empty() const { return m_size == 0; }

      constexpr size_t size() const { return m_size; }
      constexpr T const *data() const { return m_ptr; }

      constexpr T const &operator[](size_t i) const { return m_ptr[i]; }

      constexpr const_iterator begin() const noexcept { return m_ptr; }
      constexpr const_iterator cbegin() const noexcept { return m_ptr; }

      constexpr const_iterator end() const noexcept { return m_ptr + m_size; }
      constexpr const_iterator cend() const noexcept { return m_ptr + m_size; }

      size_t find(T c, size_t pos = 0) const;
      size_t find(const T *s, size_t pos = 0) const;

      size_t find_first_of(T c, size_t pos = 0) const;
      size_t find_first_of(T const *s, size_t pos = 0) const;

      size_t find_first_not_of(T c, size_t pos = 0) const;
      size_t find_first_not_of(T const *s, size_t pos = 0) const;

      size_t find_last_of(T c, size_t pos = npos) const;
      size_t find_last_of(T const *s, size_t pos = npos) const;

      size_t find_last_not_of(T c, size_t pos = npos) const;
      size_t find_last_not_of(T const *s, size_t pos = npos) const;

      basic_string_view substr(size_t pos = 0, size_t count = npos) const;

      void remove_prefix(size_t n);
      void remove_suffix(size_t n);

      static constexpr size_t npos = size_t(-1);

    public:

      std::basic_string<T, traits> to_string() const { return std::basic_string<T, traits>(data(), size()); }

    private:

      T const *m_ptr = nullptr;
      size_t m_size = 0;
  };

  template<typename T, class traits>
  constexpr basic_string_view<T, traits>::basic_string_view(T const *str)
    : m_ptr(str), m_size(traits::length(str))
  {
  }

  template<typename T, class traits>
  constexpr basic_string_view<T, traits>::basic_string_view(T const *str, size_t len)
    : m_ptr(str), m_size(len)
  {
  }

  template<typename T, class traits>
  constexpr basic_string_view<T, traits>::basic_string_view(std::basic_string<T, traits> const &str)
    : m_ptr(str.data()), m_size(str.size())
  {
  }

  template<typename T, class traits>
  size_t basic_string_view<T, traits>::find(T c, size_t pos) const
  {
    for(auto i = pos; i < m_size; ++i)
    {
      if (m_ptr[i] == c)
        return i;
    }

    return npos;
  }

  template<typename T, class traits>
  size_t basic_string_view<T, traits>::find(const T *s, size_t pos) const
  {
    for(auto i = pos; i < m_size - std::min(m_size, traits::length(s)); ++i)
    {
      bool found = true;
      for(auto j = 0; found && s[j] != 0; ++j)
      {
        found &= (m_ptr[i+j] == s[+j]);
      }

      if (found)
        return i;
    }

    return npos;
  }

  template<typename T, class traits>
  size_t basic_string_view<T, traits>::find_first_of(T c, size_t pos) const
  {
    for(auto i = pos; i < m_size; ++i)
    {
      if (m_ptr[i] == c)
        return i;
    }

    return npos;
  }

  template<typename T, class traits>
  size_t basic_string_view<T, traits>::find_first_of(T const *s, size_t pos) const
  {
    for(auto i = pos; i < m_size; ++i)
    {
      for(auto c = s; *c != 0; ++c)
      {
        if (m_ptr[i] == *c)
          return i;
      }
    }

    return npos;
  }

  template<typename T, class traits>
  size_t basic_string_view<T, traits>::find_first_not_of(T c, size_t pos) const
  {
    for(auto i = pos; i < m_size; ++i)
    {
      bool found = (m_ptr[i] == c);

      if (!found)
        return i;
    }

    return npos;
  }

  template<typename T, class traits>
  size_t basic_string_view<T, traits>::find_first_not_of(T const *s, size_t pos) const
  {
    for(auto i = pos; i < m_size; ++i)
    {
      bool found = false;
      for(auto c = s; !found && *c != 0; ++c)
      {
        found |= (m_ptr[i] == *c);
      }

      if (!found)
        return i;
    }

    return npos;
  }

  template<typename T, class traits>
  size_t basic_string_view<T, traits>::find_last_of(T c, size_t pos) const
  {
    for(int i = std::min(std::max(m_size, size_t(1)) - 1, pos); i > 0; --i)
    {
      if (m_ptr[i] == c)
        return i;
    }

    return npos;
  }

  template<typename T, class traits>
  size_t basic_string_view<T, traits>::find_last_of(T const *s, size_t pos) const
  {
    for(int i = std::min(std::max(m_size, size_t(1)) - 1, pos); i > 0; --i)
    {
      for(auto c = s; *c != 0; ++c)
      {
        if (m_ptr[i] == *c)
          return i;
      }
    }

    return npos;
  }

  template<typename T, class traits>
  size_t basic_string_view<T, traits>::find_last_not_of(T c, size_t pos) const
  {
    for(int i = std::min(std::max(m_size, size_t(1)) - 1, pos); i > 0; --i)
    {
      bool found = (m_ptr[i] == c);

      if (!found)
        return i;
    }

    return npos;
  }

  template<typename T, class traits>
  size_t basic_string_view<T, traits>::find_last_not_of(T const *s, size_t pos) const
  {
    for(int i = std::min(std::max(m_size, size_t(1)) - 1, pos); i > 0; --i)
    {
      bool found = false;
      for(auto c = s; !found && *c != 0; ++c)
      {
        found |= (m_ptr[i] == *c);
      }

      if (!found)
        return i;
    }

    return npos;
  }

  template<typename T, class traits>
  basic_string_view<T, traits> basic_string_view<T, traits>::substr(size_t pos, size_t count) const
  {
    if (pos > m_size)
    {
      throw std::out_of_range("string_view pos out of range");
    }

    return { m_ptr + pos, std::min(count, pos > m_size ? 0 : m_size - pos) };
  }

  template<typename T, class traits>
  void basic_string_view<T, traits>::remove_prefix(size_t n)
  {
    m_ptr += n;
    m_size -= n;
  }

  template<typename T, class traits>
  void basic_string_view<T, traits>::remove_suffix(size_t n)
  {
    m_size -= n;
  }

  template<typename T, class traits>
  constexpr bool operator ==(basic_string_view<T, traits> const &lhs, basic_string_view<T, traits> const &rhs)
  {
    return (lhs.size() == rhs.size()) && (traits::compare(lhs.data(), rhs.data(), std::min(lhs.size(), rhs.size())) == 0);
  }


  template<typename T, class traits>
  constexpr bool operator ==(basic_string_view<T, traits> const &lhs, std::decay_t<basic_string_view<T, traits>> const &rhs)
  {
    return (lhs == rhs);
  }


  template<typename T, class traits>
  constexpr bool operator ==(std::decay_t<basic_string_view<T, traits>> const &lhs, basic_string_view<T, traits> const &rhs)
  {
    return (lhs == rhs);
  }


  template<typename T, class traits>
  constexpr bool operator !=(basic_string_view<T, traits> const &lhs, basic_string_view<T, traits> const &rhs)
  {
    return !(lhs == rhs);
  }


  template<typename T, class traits>
  constexpr bool operator !=(basic_string_view<T, traits> const &lhs, std::decay_t<basic_string_view<T, traits>> const &rhs)
  {
    return !(lhs == rhs);
  }

  template<typename T, class traits>
  constexpr bool operator !=(std::decay_t<basic_string_view<T, traits>> const &lhs, basic_string_view<T, traits> const &rhs)
  {
    return !(lhs == rhs);
  }

  template<typename T, class traits>
  constexpr bool operator <(basic_string_view<T, traits> const &lhs, basic_string_view<T, traits> const &rhs)
  {
    auto comp = traits::compare(lhs.data(), rhs.data(), std::min(lhs.size(), rhs.size()));

    return comp ? (comp < 0) : (lhs.size() < rhs.size());
  }


  template<typename T, class traits>
  constexpr bool operator <(basic_string_view<T, traits> const &lhs, std::decay_t<basic_string_view<T, traits>> const &rhs)
  {
    return (lhs < rhs);
  }


  template<typename T, class traits>
  constexpr bool operator <(std::decay_t<basic_string_view<T, traits>> const &lhs, basic_string_view<T, traits> const &rhs)
  {
    return (lhs < rhs);
  }


  typedef basic_string_view<char> string_view;
  typedef basic_string_view<wchar_t> wstring_view;

} // namespace

#endif
