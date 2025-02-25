#pragma once

#include <bitset>
#include <climits>
#include <type_traits>

template <typename T>
class Bitflags {
  public:
    using utype = std::underlying_type<T>::type;

    void Set(T t)
    {
        mBitset.set(static_cast<utype>(t));
    }

    void Unset(T t)
    {
        mBitset.reset(static_cast<utype>(t));
    }

    void SetAll()
    {
        mBitset.set();
    }

    void Clear()
    {
        mBitset.reset();
    }

    bool operator[](T t) const
    {
        return mBitset[static_cast<utype>(t)];
    }

    [[nodiscard]] bool All() const
    {
        return mBitset.all();
    }

    [[nodiscard]] bool None() const
    {
        return mBitset.none();
    }

    [[nodiscard]] bool Any() const
    {
        return mBitset.any();
    }

    [[nodiscard]] std::string to_string() const
    {
        return mBitset.to_string();
    }

  private:
    std::bitset<CHAR_BIT * sizeof(utype)> mBitset;
};