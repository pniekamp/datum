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

    template<class Func>
    delegate(Func func)
    {
      static_assert(sizeof(func) <= sizeof(storage), "size");

      new(storage) holder<Func>(std::move(func));
    }

    R operator()(Args... args) const
    {
      return static_cast<holderbase const *>(data())->invoke(std::forward<Args>(args)...);
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

        holder(T &&value)
          : held(std::forward<T>(value))
        {
        }

        R invoke(Args&&... args) const
        {
          return held(std::forward<Args>(args)...);
        }

        T held;
    };

    void *data() { return static_cast<void *>(&storage); }
    void const *data() const { return static_cast<void const *>(&storage); }

    alignas(alignof(std::max_align_t)) char storage[64];
};
