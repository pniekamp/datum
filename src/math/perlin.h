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
      result_type operator()(T x, T y, T z, T t) const;

    private:

      T dotgrad(uint32_t p, T x) const;
      T dotgrad(uint32_t p, T x, T y) const;
      T dotgrad(uint32_t p, T x, T y, T z) const;
      T dotgrad(uint32_t p, T x, T y, T z, T t) const;

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

  template<typename T, size_t N, size_t TableSize>
  T perlin_engine<T, N, TableSize>::dotgrad(uint32_t p, T x, T y, T z, T t) const
  {
    switch (p & 31)
    {
      case 0: return x + y + z; // (1,1,1,0)
      case 1: return -x + y + z; // (-1,1,1,0)
      case 2: return x - y + z; // (1,-1,1,0)
      case 3: return -x - y + z; // (-1,-1,1,0)
      case 4: return x + y - z; // (1,1,-1,0)
      case 5: return -x + y - z; // (-1,1,-1,0)
      case 6: return x - y - z; // (1,-1,-1,0)
      case 7: return -x - y - z; // (-1,-1,-1,0)
      case 8: return x + y + t; // (1,1,0,1)
      case 9: return -x + y + t; // (-1,1,0,1)
      case 10: return x - y + t; // (1,-1,0,1)
      case 11: return -x - y + t; // (-1,-1,0,1)
      case 12: return x + y - t; // (1,1,0,-1)
      case 13: return -x + y - t; // (-1,1,0,-1)
      case 14: return x - y - t; // (1,-1,0,-1)
      case 15: return -x - y - t; // (-1,-1,0,-1)
      case 16: return x + z + t; // (1,0,1,1)
      case 17: return -x + z + t; // (-1,0,1,1)
      case 18: return x - z + t; // (1,0,-1,1)
      case 19: return -x - z + t; // (-1,0,-1,1)
      case 20: return x + z - t; // (1,0,1,-1)
      case 21: return -x + z - t; // (-1,0,1,-1)
      case 22: return x - z - t; // (1,0,-1,-1)
      case 23: return -x - z - t; // (-1,0,-1,-1)
      case 24: return y + z + t; // (0,1,1,1)
      case 25: return -y + z + t; // (0,-1,1,1)
      case 26: return y - z + t; // (0,1,-1,1)
      case 27: return -y - z + t; // (0,-1,-1,1)
      case 28: return y + z - t; // (0,1,1,-1)
      case 29: return -y + z - t; // (0,-1,1,-1)
      case 30: return y - z - t; // (0,1,-1,-1)
      case 31: return -y - z - t; // (0,-1,-1,-1)
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

    auto d0 = dotgrad(hash(ix), tx);
    auto d1 = dotgrad(hash(ix+1), tx-1);

    auto du = 30 * tx * tx * (tx * (tx - 2) + 1);

    auto k0 = d0;
    auto k1 = (d1 - d0);

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

    auto d00 = dotgrad(hash(ix, iy), tx, ty);
    auto d10 = dotgrad(hash(ix+1, iy), tx-1, ty);
    auto d01 = dotgrad(hash(ix, iy+1), tx, ty-1);
    auto d11 = dotgrad(hash(ix+1, iy+1), tx-1, ty-1);

    auto du = 30 * tx * tx * (tx * (tx - 2) + 1);
    auto dv = 30 * ty * ty * (ty * (ty - 2) + 1);

    auto k0 = d00;
    auto k1 = (d10 - d00);
    auto k2 = (d01 - d00);
    auto k3 = (d00 + d11 - d10 - d01);

    result_type result;
    result.noise = k0 + k1 * u + k2 * v + k3 * u * v;
    result.derivative(0) = du * (k1 + k3 * v);
    result.derivative(1) = dv * (k2 + k3 * u);

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
    result.derivative(1) = dv * (k2 + k4 * u + k6 * w + k7 * u * w);
    result.derivative(2) = dw * (k3 + k5 * u + k6 * v + k7 * u * v);

    return result;
  }


  //|///////////////////// perlin_engine::gen /////////////////////////////////
  template<typename T, size_t N, size_t TableSize>
  typename perlin_engine<T, N, TableSize>::result_type perlin_engine<T, N, TableSize>::operator()(T x, T y, T z, T t) const
  {
    using std::floor;
    using namespace leap::lml;

    static_assert(N == 4, "invalid");

    int ix = (int)(floor(x)) & (TableSize - 1);
    int iy = (int)(floor(y)) & (TableSize - 1);
    int iz = (int)(floor(z)) & (TableSize - 1);
    int it = (int)(floor(t)) & (TableSize - 1);

    auto tx = x - floor(x);
    auto ty = y - floor(y);
    auto tz = z - floor(z);
    auto tt = t - floor(t);

    auto u = tx * tx * tx * (tx * (tx * 6 - 15) + 10);
    auto v = ty * ty * ty * (ty * (ty * 6 - 15) + 10);
    auto w = tz * tz * tz * (tz * (tz * 6 - 15) + 10);
    auto q = tt * tt * tt * (tt * (tt * 6 - 15) + 10);

    auto hash = [&](int i, int j, int k, int l) { return P[P[P[P[i] + j] + k] + l]; };

    auto d0000 = dotgrad(hash(ix, iy, iz, it), tx, ty, tz, tt);
    auto d1000 = dotgrad(hash(ix+1, iy, iz, it), tx-1, ty, tz, tt);
    auto d0100 = dotgrad(hash(ix, iy+1, iz, it), tx, ty-1, tz, tt);
    auto d1100 = dotgrad(hash(ix+1, iy+1, iz, it), tx-1, ty-1, tz, tt);
    auto d0010 = dotgrad(hash(ix, iy, iz+1, it), tx, ty, tz-1, tt);
    auto d1010 = dotgrad(hash(ix+1, iy, iz+1, it), tx-1, ty, tz-1, tt);
    auto d0110 = dotgrad(hash(ix, iy+1, iz+1, it), tx, ty-1, tz-1, tt);
    auto d1110 = dotgrad(hash(ix+1, iy+1, iz+1, it), tx-1, ty-1, tz-1, tt);
    auto d0001 = dotgrad(hash(ix, iy, iz, it+1), tx, ty, tz, tt-1);
    auto d1001 = dotgrad(hash(ix+1, iy, iz, it+1), tx-1, ty, tz, tt-1);
    auto d0101 = dotgrad(hash(ix, iy+1, iz, it+1), tx, ty-1, tz, tt-1);
    auto d1101 = dotgrad(hash(ix+1, iy+1, iz, it+1), tx-1, ty-1, tz, tt-1);
    auto d0011 = dotgrad(hash(ix, iy, iz+1, it+1), tx, ty, tz-1, tt-1);
    auto d1011 = dotgrad(hash(ix+1, iy, iz+1, it+1), tx-1, ty, tz-1, tt-1);
    auto d0111 = dotgrad(hash(ix, iy+1, iz+1, it+1), tx, ty-1, tz-1, tt-1);
    auto d1111 = dotgrad(hash(ix+1, iy+1, iz+1, it+1), tx-1, ty-1, tz-1, tt-1);

    auto du = 30 * tx * tx * (tx * (tx - 2) + 1);
    auto dv = 30 * ty * ty * (ty * (ty - 2) + 1);
    auto dw = 30 * tz * tz * (tz * (tz - 2) + 1);
    auto dq = 30 * tt * tt * (tt * (tt - 2) + 1);

    auto k0 = d0000;
    auto k1 = (d1000 - d0000);
    auto k2 = (d0100 - d0000);
    auto k3 = (d0010 - d0000);
    auto k4 = (d0001 - d0000);
    auto k5 = (d0000 + d1100 - d1000 - d0100);
    auto k6 = (d0000 + d1010 - d1000 - d0010);
    auto k7 = (d0000 + d1001 - d1000 - d0001);
    auto k8 = (d0000 + d0110 - d0100 - d0010);
    auto k9 = (d0000 + d0101 - d0100 - d0001);
    auto k10 = (d0000 + d0011 - d0010 - d0001);
    auto k11 = (d1000 + d0100 + d0010 + d1110 - d0000 - d1100 - d1010 - d0110);
    auto k12 = (d1000 + d0100 + d0001 + d1101 - d0000 - d1100 - d1001 - d0101);
    auto k13 = (d1000 + d0010 + d0001 + d1011 - d0000 - d1010 - d1001 - d0011);
    auto k14 = (d0100 + d0010 + d0111 + d0001 - d0110 - d0101 - d0000 - d0011);
    auto k15 = (d0000 + d1100 + d0011 + d0110 + d0101 + d1010 + d1001 + d1111 - d1110 - d1011 - d1000 - d0111 - d0100 - d0010 - d0001 - d1101);

    result_type result;
    result.noise = k0 + k1 * u + k2 * v + k3 * w + k4 * q + k5 * u * v + k6 * u * w + k7 * u * q + k8 * v * w + k9 * v * q + k10 * w * q + k11 * u * v * w + k12 * u * v * q + k13 * u * w * q + k14 * v * w * q + k15 * u * v * w * q;
    result.derivative(0) = du * (k1 + k5 * v + k6 * w + k7 * q + k11 * v * w + k12 * v * q + k13 * w * q + k15 * v * w * q);
    result.derivative(1) = dv * (k2 + k5 * u + k8 * w + k9 * q + k11 * u * w + k12 * u * q + k14 * w * q + k15 * u * w * q);
    result.derivative(2) = dw * (k3 + k6 * u + k8 * v + k10 * q + k11 * u * v + k13 * u * q + k14 * v * q + k15 * u * v * q);
    result.derivative(3) = dq * (k4 + k7 * u + k9 * v + k10 * w + k12 * u * v + k13 * u * w + k14 * v * w + k15 * u * v * w);

    return result;
  }

  using Perlin1f = perlin_engine<float, 1, 256>;
  using Perlin2f = perlin_engine<float, 2, 256>;
  using Perlin3f = perlin_engine<float, 3, 256>;
  using Perlin4f = perlin_engine<float, 4, 256>;
  using Perlin1d = perlin_engine<double, 1, 256>;
  using Perlin2d = perlin_engine<double, 2, 256>;
  using Perlin3d = perlin_engine<double, 3, 256>;
  using Perlin4d = perlin_engine<double, 4, 256>;
}
