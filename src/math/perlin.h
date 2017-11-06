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

      struct result_type
      {
        T noise;
        Vector<T, N> derivative;
      };

    public:
      explicit perlin_engine(std::mt19937 &entropy);
      explicit perlin_engine(std::uint_fast32_t value = 1234);

      void seed(std::mt19937 &entropy);
      void seed(std::uint_fast32_t value);

      result_type operator()(T x) const;
      result_type operator()(T x, T y) const;
      result_type operator()(T x, T y, T z) const;

    private:

      T dotgrad(uint32_t p, T x) const;
      T dotgrad(uint32_t p, T x, T y) const;
      T dotgrad(uint32_t p, T x, T y, T z) const;

      uint32_t P[2*TableSize+1];
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
    std::uniform_real_distribution<T> real11{T(-1), T(1)};

    std::iota(P, P + TableSize, 0);

    std::shuffle(P, P + TableSize, entropy);

    std::copy(P, P + TableSize, P + TableSize);
    std::copy(P, P + 1, P + 2*TableSize);
  }

  template<typename T, size_t N, size_t TableSize>
  void perlin_engine<T, N, TableSize>::seed(std::uint_fast32_t value)
  {
    std::mt19937 entropy(value);

    seed(entropy);
  }


  //|///////////////////// perlin_engine::dotgrad /////////////////////////////
  template<typename T, size_t N, size_t TableSize>
  T perlin_engine<T, N, TableSize>::dotgrad(uint32_t p, T x) const
  {
    switch (p & 1)
    {
      case 0: return x; // (1)
      case 1: return -x; // (-1)
    }

    return 0;
  }

  template<typename T, size_t N, size_t TableSize>
  T perlin_engine<T, N, TableSize>::dotgrad(uint32_t p, T x, T y) const
  {
    switch (p & 3)
    {
      case 0: return x + y; // (1,1)
      case 1: return -x + y; // (-1,1)
      case 2: return x - y; // (1,-1)
      case 3: return -x - y; // (-1,-1)
    }

    return 0;
  }

  template<typename T, size_t N, size_t TableSize>
  T perlin_engine<T, N, TableSize>::dotgrad(uint32_t p, T x, T y, T z) const
  {
    switch (p & 15)
    {
      case 0: return x + y; // (1,1,0)
      case 1: return -x + y; // (-1,1,0)
      case 2: return x - y; // (1,-1,0)
      case 3: return -x - y; // (-1,-1,0)
      case 4: return x + z; // (1,0,1)
      case 5: return -x + z; // (-1,0,1)
      case 6: return x - z; // (1,0,-1)
      case 7: return -x - z; // (-1,0,-1)
      case 8: return y + z; // (0,1,1),
      case 9: return -y + z; // (0,-1,1),
      case 10: return y - z; // (0,1,-1),
      case 11: return -y - z; // (0,-1,-1)

      case 12: return y + x; // (1,1,0)
      case 13: return -x + y; // (-1,1,0)
      case 14: return -y + z; // (0,-1,1)
      case 15: return -y - z; // (0,-1,-1)
    }

    return 0;
  }


  //|///////////////////// perlin_engine::gen /////////////////////////////////
  template<typename T, size_t N, size_t TableSize>
  typename perlin_engine<T, N, TableSize>::result_type perlin_engine<T, N, TableSize>::operator()(T x) const
  {
    using std::floor;
    using namespace leap::lml;

    static_assert(N == 1, "invalid");

    int ix = (int)(floor(x)) & (TableSize - 1);

    auto tx = x - floor(x);

    auto u = tx * tx * tx * (tx * (tx * 6 - 15) + 10);

    auto hash = [&](int i) { return P[i]; };

    auto d000 = dotgrad(hash(ix), tx);
    auto d100 = dotgrad(hash(ix+1), tx-1);

    auto du = 30 * tx * tx * (tx * (tx - 2) + 1);

    auto k0 = d000;
    auto k1 = (d100 - d000);

    result_type result;
    result.noise = k0 + k1 * u;
    result.derivative(0) = du * k1;

    return result;
  }


  //|///////////////////// perlin_engine::gen /////////////////////////////////
  template<typename T, size_t N, size_t TableSize>
  typename perlin_engine<T, N, TableSize>::result_type perlin_engine<T, N, TableSize>::operator()(T x, T y) const
  {
    using std::floor;
    using namespace leap::lml;

    static_assert(N == 2, "invalid");

    int ix = (int)(floor(x)) & (TableSize - 1);
    int iy = (int)(floor(y)) & (TableSize - 1);

    auto tx = x - floor(x);
    auto ty = y - floor(y);

    auto u = tx * tx * tx * (tx * (tx * 6 - 15) + 10);
    auto v = ty * ty * ty * (ty * (ty * 6 - 15) + 10);

    auto hash = [&](int i, int j) { return P[P[i] + j]; };

    auto d000 = dotgrad(hash(ix, iy), tx, ty);
    auto d100 = dotgrad(hash(ix+1, iy), tx-1, ty);
    auto d010 = dotgrad(hash(ix, iy+1), tx, ty-1);
    auto d110 = dotgrad(hash(ix+1, iy+1), tx-1, ty-1);

    auto du = 30 * tx * tx * (tx * (tx - 2) + 1);
    auto dv = 30 * ty * ty * (ty * (ty - 2) + 1);

    auto k0 = d000;
    auto k1 = (d100 - d000);
    auto k2 = (d010 - d000);
    auto k4 = (d000 + d110 - d100 - d010);

    result_type result;
    result.noise = k0 + k1 * u + k2 * v + k4 * u * v;
    result.derivative(0) = du * (k1 + k4 * v);
    result.derivative(1) = dv * (k2 + k4 * u);

    return result;
  }


  //|///////////////////// perlin_engine::gen /////////////////////////////////
  template<typename T, size_t N, size_t TableSize>
  typename perlin_engine<T, N, TableSize>::result_type perlin_engine<T, N, TableSize>::operator()(T x, T y, T z) const
  {
    using std::floor;
    using namespace leap::lml;

    static_assert(N == 3, "invalid");

    int ix = (int)(floor(x)) & (TableSize - 1);
    int iy = (int)(floor(y)) & (TableSize - 1);
    int iz = (int)(floor(z)) & (TableSize - 1);

    auto tx = x - floor(x);
    auto ty = y - floor(y);
    auto tz = z - floor(z);

    auto u = tx * tx * tx * (tx * (tx * 6 - 15) + 10);
    auto v = ty * ty * ty * (ty * (ty * 6 - 15) + 10);
    auto w = tz * tz * tz * (tz * (tz * 6 - 15) + 10);

    auto hash = [&](int i, int j, int k) { return P[P[P[i] + j] + k]; };

    auto d000 = dotgrad(hash(ix, iy, iz), tx, ty, tz);
    auto d100 = dotgrad(hash(ix+1, iy, iz), tx-1, ty, tz);
    auto d010 = dotgrad(hash(ix, iy+1, iz), tx, ty-1, tz);
    auto d110 = dotgrad(hash(ix+1, iy+1, iz), tx-1, ty-1, tz);
    auto d001 = dotgrad(hash(ix, iy, iz+1), tx, ty, tz-1);
    auto d101 = dotgrad(hash(ix+1, iy, iz+1), tx-1, ty, tz-1);
    auto d011 = dotgrad(hash(ix, iy+1, iz+1), tx, ty-1, tz-1);
    auto d111 = dotgrad(hash(ix+1, iy+1, iz+1), tx-1, ty-1, tz-1);

    auto du = 30 * tx * tx * (tx * (tx - 2) + 1);
    auto dv = 30 * ty * ty * (ty * (ty - 2) + 1);
    auto dw = 30 * tz * tz * (tz * (tz - 2) + 1);

    auto k0 = d000;
    auto k1 = (d100 - d000);
    auto k2 = (d010 - d000);
    auto k3 = (d001 - d000);
    auto k4 = (d000 + d110 - d100 - d010);
    auto k5 = (d000 + d101 - d100 - d001);
    auto k6 = (d000 + d011 - d010 - d001);
    auto k7 = (d100 + d010 + d001 + d111 - d000 - d110 - d101 - d011);

    result_type result;
    result.noise = k0 + k1 * u + k2 * v + k3 * w + k4 * u * v + k5 * u * w + k6 * v * w + k7 * u * v * w;
    result.derivative(0) = du * (k1 + k4 * v + k5 * w + k7 * v * w);
    result.derivative(1) = dv * (k2 + k4 * u + k6 * w + k7 * v * w);
    result.derivative(2) = dw * (k3 + k5 * u + k6 * v + k7 * v * w);

    return result;
  }

  typedef perlin_engine<float, 1, 256> Perlin1f;
  typedef perlin_engine<float, 2, 256> Perlin2f;
  typedef perlin_engine<float, 3, 256> Perlin3f;
  typedef perlin_engine<double, 1, 256> Perlin1d;
  typedef perlin_engine<double, 2, 256> Perlin2d;
  typedef perlin_engine<double, 3, 256> Perlin3d;
}
