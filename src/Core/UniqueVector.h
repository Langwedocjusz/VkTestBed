#pragma once

#include <vector>
#include <algorithm>

template <typename T>
class UniqueVector{
public:
    size_t insert(const T& t)
    {
        //To-do: maybe keep mData sorted to lower complexity
        //of search to O(log(N))
        auto it = std::find(mData.begin(), mData.end(), t);

        if(it != mData.end())
        {
            return it - mData.begin();
        }
        else
        {
            mData.push_back(t);
            return mData.size() - 1;
        }
    }

    const std::vector<T>& data() const
    {
        return mData;
    }

private:
    std::vector<T> mData;
};