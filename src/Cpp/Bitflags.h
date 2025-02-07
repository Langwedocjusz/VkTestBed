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

    bool operator[](T t)
    {
        return mBitset[static_cast<utype>(t)];
    }

    bool All()
    {
        return mBitset.all();
    }

    bool None()
    {
        return mBitset.none();
    }

    bool Any()
    {
        return mBitset.any();
    }

    std::string to_string()
    {
        return mBitset.to_string();
    }

  private:
    std::bitset<CHAR_BIT * sizeof(utype)> mBitset;
};