//
// delegate
//

//
// Copyright (c) 2016 Peter Niekamp
//

#pragma once

#include <cstddef>
#include <functional>

//|---------------------- delegate ------------------------------------------
//|--------------------------------------------------------------------------

template<typename R, typename... Args> class delegate;
template<typename R, typename... Args> class delegate<R(Args...)>
{
  public:
    delegate() = default;

    template<class Lambda>
    delegate(Lambda const &lambda)
      : valid(true)
    {
      static_assert(sizeof(lambda) <= sizeof(storage), "size");

      new(storage) holder<Lambda>(lambda);
    }

    operator bool() const { return valid; }

    R operator()(Args... args) const
    {
      return reinterpret_cast<holderbase const *>(storage)->invoke(std::forward<Args>(args)...);
    }

  private:

    struct holderbase
    {
      virtual ~holderbase() = default;
      virtual R invoke(Args&&... args) const = 0;
    };

    template<typename T>
    class holder : public holderbase
    {
      public:

        holder(T value)
          : held(std::move(value))
        {
        }

        R invoke(Args&&... args) const
        {
          return held(std::forward<Args>(args)...);
        }

        T held;
    };

    bool valid = false;
    alignas(alignof(std::max_align_t)) char storage[64];
};
