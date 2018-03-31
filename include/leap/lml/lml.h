//
// lml
//
// Copyright (C) Peter Niekamp
//
// This code remains the property of the copyright holder.
// The code contained herein may be used for any purpose provided
// this copyright notice is retained
//

#pragma once

#include <leap/util.h>

/**
 * \mainpage Leap Math Library
 *
 * \section intro_sec Introduction
 *
 * The Leap Library is a collection of useful math routines
 *
 */

namespace leap { namespace lml
{
  // bring in some of the math utils

  using leap::fcmp;
  using leap::fmod2;
  using leap::sign;
  using leap::clamp;
  using leap::lerp;
  using leap::remap;

  template<typename... T, std::enable_if_t<std::is_arithmetic<std::common_type_t<T...>>::value>* = nullptr>
  auto min(T&&... args)
  {
    using std::min;

    return min(args...);
  }

  template<typename... T, std::enable_if_t<std::is_arithmetic<std::common_type_t<T...>>::value>* = nullptr>
  auto max(T&&... args)
  {
    using std::max;

    return max(args...);
  }

} }
