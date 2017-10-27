//
// Datum - perlin noise
//

//
// Copyright (c) 2017 Peter Niekamp
//

#pragma once

#include "vec.h"
#include <numeric>
#include <algorithm>
#include <random>

namespace lml
{

  //|---------------------- perlin_engine -------------------------------------
  //|--------------------------------------------------------------------------

  template<typename T, size_t N, size_t TableSize>
  class perlin_engine
  {
    public:

      typedef T result_type;

    public:
      explicit perlin_engine(std::mt19937 &entropy);
      explicit perlin_engine(std::uint_fast32_t value = 1234);

      void seed(std::mt19937 &entropy);
      void seed(std::uint_fast32_t value);

      T operator()(T x);
      T operator()(T x, T y);
      T operator()(T x, T y, T z);

    private:

      uint32_t P[2*TableSize];

      lml::Vector<T, N> G[TableSize];
  };


  //|///////////////////// perlin_engine::Constructor /////////////////////////
  template<typename T, size_t N, size_t TableSize>
  perlin_engine<T, N, TableSize>::perlin_engine(std::mt19937 &entropy)
  {
    seed(entropy);
  }


  //|///////////////////// perlin_engine::Constructor /////////////////////////
  template<typename T, size_t N, size_t TableSize>
  perlin_engine<T, N, TableSize>::perlin_engine(std::uint_fast32_t value)
  {
    seed(value);
  }


  //|///////////////////// perlin_engine::seed ////////////////////////////////
  template<typename T, size_t N, size_t TableSize>
  void perlin_engine<T, N, TableSize>::seed(std::mt19937 &entropy)
  {
    std::uniform_real_distribution<T> real11{-1.0f, 1.0f};

    for(size_t i = 0; i < TableSize; ++i)
    {
      do
      {
        for(size_t k = 0; k < N; ++k)
        {
          G[i][k] = real11(entropy);
        }

      } while (normsqr(G[i]) > 1);

      G[i] = normalise(G[i]);
      P[i] = i;
    }

    std::shuffle(P, P + TableSize, entropy);

    std::copy(P, P + TableSize, P + TableSize);
  }


  //|///////////////////// perlin_engine::seed ////////////////////////////////
  template<typename T, size_t N, size_t TableSize>
  void perlin_engine<T, N, TableSize>::seed(std::uint_fast32_t value)
  {
    std::mt19937 entropy(value);

    seed(entropy);
  }


  //|///////////////////// perlin_engine::gen /////////////////////////////////
  template<typename T, size_t N, size_t TableSize>
  T perlin_engine<T, N, TableSize>::operator()(T x)
  {
    using std::floor;
    using namespace leap::lml;

    static_assert(N == 1, "invalid");

    int x0 = (int)(floor(x)) & (TableSize - 1);
    int x1 = (int)(x0 + 1) & (TableSize - 1);

    auto tx = x - floor(x);

    auto u = tx * tx * tx * (tx * (tx * 6 - 15) + 10);

    auto hash = [&](int i) { return P[i]; };

    auto d000 = dot(G[hash(x0)], Vector<T, N>{tx});
    auto d100 = dot(G[hash(x1)], Vector<T, N>{tx-1});

    return lerp(d000, d100, u);
  }


  //|///////////////////// perlin_engine::gen /////////////////////////////////
  template<typename T, size_t N, size_t TableSize>
  T perlin_engine<T, N, TableSize>::operator()(T x, T y)
  {
    using std::floor;
    using namespace leap::lml;

    static_assert(N == 2, "invalid");

    int x0 = (int)(floor(x)) & (TableSize - 1);
    int x1 = (int)(x0 + 1) & (TableSize - 1);
    int y0 = (int)(floor(y)) & (TableSize - 1);
    int y1 = (int)(y0 + 1) & (TableSize - 1);

    auto tx = x - floor(x);
    auto ty = y - floor(y);

    auto u = tx * tx * tx * (tx * (tx * 6 - 15) + 10);
    auto v = ty * ty * ty * (ty * (ty * 6 - 15) + 10);

    auto hash = [&](int i, int j) { return P[P[i] + j]; };

    auto d000 = dot(G[hash(x0, y0)], Vector<T, N>{tx, ty});
    auto d100 = dot(G[hash(x1, y0)], Vector<T, N>{tx-1, ty});
    auto d010 = dot(G[hash(x0, y1)], Vector<T, N>{tx, ty-1});
    auto d110 = dot(G[hash(x1, y1)], Vector<T, N>{tx-1, ty-1});

    return lerp(lerp(d000, d100, u), lerp(d010, d110, u), v);
  }


  //|///////////////////// perlin_engine::gen /////////////////////////////////
  template<typename T, size_t N, size_t TableSize>
  T perlin_engine<T, N, TableSize>::operator()(T x, T y, T z)
  {
    using std::floor;
    using namespace leap::lml;

    static_assert(N == 3, "invalid");

    int x0 = (int)(floor(x)) & (TableSize - 1);
    int x1 = (int)(x0 + 1) & (TableSize - 1);
    int y0 = (int)(floor(y)) & (TableSize - 1);
    int y1 = (int)(y0 + 1) & (TableSize - 1);
    int z0 = (int)(floor(z)) & (TableSize - 1);
    int z1 = (int)(z0 + 1) & (TableSize - 1);

    auto tx = x - floor(x);
    auto ty = y - floor(y);
    auto tz = z - floor(z);

    auto u = tx * tx * tx * (tx * (tx * 6 - 15) + 10);
    auto v = ty * ty * ty * (ty * (ty * 6 - 15) + 10);
    auto w = tz * tz * tz * (tz * (tz * 6 - 15) + 10);

    auto hash = [&](int i, int j, int k) { return P[P[P[i] + j] + k]; };

    auto d000 = dot(G[hash(x0, y0, z0)], Vector<T, N>{tx, ty, tz});
    auto d100 = dot(G[hash(x1, y0, z0)], Vector<T, N>{tx-1, ty, tz});
    auto d010 = dot(G[hash(x0, y1, z0)], Vector<T, N>{tx, ty-1, tz});
    auto d110 = dot(G[hash(x1, y1, z0)], Vector<T, N>{tx-1, ty-1, tz});
    auto d001 = dot(G[hash(x0, y0, z1)], Vector<T, N>{tx, ty, tz-1});
    auto d101 = dot(G[hash(x1, y0, z1)], Vector<T, N>{tx-1, ty, tz-1});
    auto d011 = dot(G[hash(x0, y1, z1)], Vector<T, N>{tx, ty-1, tz-1});
    auto d111 = dot(G[hash(x1, y1, z1)], Vector<T, N>{tx-1, ty-1, tz-1});

    return lerp(lerp(lerp(d000, d100, u), lerp(d010, d110, u), v), lerp(lerp(d001, d101, u), lerp(d011, d111, u), v), w);
  }

  typedef perlin_engine<float, 1, 256> Perlin1f;
  typedef perlin_engine<float, 2, 256> Perlin2f;
  typedef perlin_engine<float, 3, 256> Perlin3f;
  typedef perlin_engine<double, 1, 256> Perlin1d;
  typedef perlin_engine<double, 2, 256> Perlin2d;
  typedef perlin_engine<double, 3, 256> Perlin3d;
}
