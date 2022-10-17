#include <iostream>
#include <random>
#include <chrono>
#include <utility>
#include <execution>
#include <algorithm>
#include <cassert>
#include <thread>
#include <mutex>
#include <atomic>
#include <string.h>

#  if defined(_MSC_VER)
#    define __FORCEINLINE__ __forceinline
#  elif defined(__GNUC__) && __GNUC__ > 3
// Clang also defines __GNUC__ (as 4)
#    define __FORCEINLINE__ inline __attribute__ ((__always_inline__))
#  else
#    define __FORCEINLINE__ inline
#  endif


struct CStrComparator
{
    bool operator ()(const char* s1, const char* s2) const
    {
        return strcmp(s1, s2) < 0; // TODO: consider using a customized version e.g. to avoid function call to strcmp()
    }
};

class HashMap
{     
    struct HashMapElem
    {
        using HashElemSizeT = unsigned int;

        HashElemSizeT size;
        HashElemSizeT capacity;
        const char** array;
        HashMapElem(const char** arr, HashElemSizeT size, HashElemSizeT capacity) : array(arr), size(size), capacity(capacity){
        }
    };

    static const size_t LEN = 64 * 64 * 64; 
    static const size_t CHARS_IN_HASH = 3;
    using hash_t = unsigned int;

    std::vector<HashMapElem> hash_table_;
    //HashMapElem hash_table_[LEN];

    const char** array_for_placement_new_;
    size_t array_for_placement_new_size_;
    size_t array_for_placement_new_size_used_;

public:
    HashMap(size_t estimatedMaxNumOfElements = 0)
    {
        hash_table_.reserve(LEN);

        size_t avg_elements_per_bucket = estimatedMaxNumOfElements / LEN;
        array_for_placement_new_size_ = estimatedMaxNumOfElements * 3;
        array_for_placement_new_size_used_ = 0;
        array_for_placement_new_ = new const char* [array_for_placement_new_size_];
        auto ptr = array_for_placement_new_;
        for (size_t i = 0; i < LEN; ++i)
        {
            hash_table_.emplace_back(ptr, 0, avg_elements_per_bucket * 2);
            //hash_table_[i].set(ptr, 0, avg_elements_per_bucket * 2);
            ptr += avg_elements_per_bucket * 2;
            assert(ptr < array_for_placement_new_ + array_for_placement_new_size_);             
        }
        array_for_placement_new_size_used_ = ptr - array_for_placement_new_;

    }
    HashMap(const HashMap& o) = delete;
    //HashMap(HashMap&& o) = delete; // we can move - no need to delete this version of constructor
    HashMap& operator = (const HashMap& o) = delete;
    ~HashMap() // no need for virtual destructor
    {
        clear();
        for (size_t i = 0; i < LEN; ++i)
            if (hash_table_[i].array < array_for_placement_new_ || hash_table_[i].array >= array_for_placement_new_ + array_for_placement_new_size_)
                delete[] hash_table_[i].array;
    }
    void clear()
    {
        delete[] array_for_placement_new_;
    }
    __FORCEINLINE__ void add(const char* str)
    {
        //
        // TODO: Maybe do it in two passes? First calculate (in multiple threads) all the hashes (and their counters - how many times a gives hash appeated)
        // and then, also in multiple threads, process these hashes - every thread different hashes - this will let avoid need for thread synchronizion
        // 

        auto hash = calcHash(str);
        assert(hash < LEN);

        auto& elem = hash_table_[hash];
        if (elem.size == elem.capacity) [[unlikely]]
            extend(hash);

        elem.array[elem.size++] = str;        
    }    
private:
    void extend(hash_t hash)
    {
        auto& elem = hash_table_[hash];
        assert(elem.size == elem.capacity);

        size_t buf_left = array_for_placement_new_size_ - array_for_placement_new_size_used_;
        size_t buf_needed = elem.size * 2;
        if (buf_needed <= buf_left)
        {
            const char** ptr = array_for_placement_new_ + array_for_placement_new_size_used_;
            array_for_placement_new_size_used_ += buf_needed;
            memcpy(ptr, elem.array, elem.size * sizeof(const char*));
            assert(elem.array >= array_for_placement_new_ && elem.array < array_for_placement_new_ + array_for_placement_new_size_);
            elem.array = ptr;
        }
        else
        {
            const char** ptr = new const char* [buf_needed];
            memcpy(ptr, elem.array, elem.size * sizeof(const char*));
            if (elem.array < array_for_placement_new_ || elem.array >= array_for_placement_new_ + array_for_placement_new_size_)
                delete[] elem.array;
            elem.array = ptr;
        }
        elem.capacity = buf_needed;
    }
    void sort_thread(size_t first_idx, size_t last_idx, std::atomic<size_t>& threads_complete)
    {
        for (size_t i = first_idx; i <= last_idx; ++i)
        {
            auto& elem = hash_table_[i];
            std::sort(elem.array, elem.array + elem.size, CStrComparator());
            //std::sort(elem.array.get(), elem.array.get() + elem.size, CStrComparator());

            auto& prev_elem = hash_table_[(i == 0) ? 0 : (i - 1)];
            assert(i == 0 || elem.size == 0 || prev_elem.size == 0 || strcmp(prev_elem.array[prev_elem.size - 1], elem.array[elem.size - 1]) < 0);
        }
        ++threads_complete;
    }
public:
    void sort()
    {
        auto num_threads = std::thread::hardware_concurrency();
        std::vector<std::thread> threads;
        threads.reserve(num_threads);

        std::atomic<size_t> threads_complete_(0);
        size_t first_idx, last_idx;
        for (unsigned int i = 0; i < num_threads; ++i)
        {
            first_idx = LEN * i / num_threads;
            last_idx = LEN * (i + 1) / num_threads - 1;
            threads.emplace_back(&HashMap::sort_thread, this, first_idx, last_idx, std::ref(threads_complete_));
        }

        for (unsigned int i = 0; i < num_threads; ++i)
            threads[i].join();

    }
    using iterator = std::pair<size_t, HashMapElem::HashElemSizeT> ;
    std::pair<const char*, iterator> getFirst()
    {
        for (size_t i = 0; i < LEN; ++i)
        {
            auto& elem = hash_table_[i];
            if (elem.size)
                return std::make_pair(elem.array[0], iterator(i, 0));
        }
        return std::make_pair(nullptr, iterator(0, 0));
    }
    std::pair<const char*, iterator> getNext(iterator it)
    {
        HashMapElem::HashElemSizeT idx = it.second+1;
        for (size_t i = it.first; i < LEN; ++i, idx = 0)
        {
            auto& elem = hash_table_[i];
            if (idx < elem.size)
                return std::make_pair(elem.array[idx], iterator(i, idx));
        }
        return std::make_pair(nullptr, iterator(0, 0));
    }
private:

    static __FORCEINLINE__ char calcAlphaHash(const char ch)
    {
        // TODO: consider a small lookup table to reduce branching
        if (ch == 0)
            return 0;
        assert((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z'));
        if (ch < 'a')
            return ch - 'A' + 1;
        else
            return ch - 'a' + 'Z' - 'A' + 2;
    }
    static __FORCEINLINE__ char getAlphaHash(const char ch)
    {
      static char hash_lookup[128] = { 0 ,0 , 0, 0, 0, 0, 0, 0, 0, 0, // it will be made by compiler just like a global array (not a method static var. with guard)
                                       0 ,0 , 0, 0, 0, 0, 0, 0, 0, 0,
                                       0 ,0 , 0, 0, 0, 0, 0, 0, 0, 0,
                                       0 ,0 , 0, 0, 0, 0, 0, 0, 0, 0,
                                       0 ,0 , 0, 0, 0, 0, 0, 0, 0, 0,
                                       0 ,0 , 0, 0, 0, 0, 0, 0, 0, 0,
                                       0 ,0 , 0, 0, 0, 1, 2, 3, 4, 5,
                                       6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
                                       16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
                                       26, 0, 0, 0, 0, 0, 0, 27, 28, 29,
                                       30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
                                       40, 41, 42, 43, 44, 45, 46, 47, 48, 49,
                                       50, 51, 52, 53, 0, 0, 0, 0 };
        
        assert(hash_lookup[ch] == calcAlphaHash(ch));
        return hash_lookup[ch];
    }
    // Calculated 24-bit hash:
    static __FORCEINLINE__ hash_t calcHash(const char* str)
    {
        static_assert(CHARS_IN_HASH <= 4, "At most 4 characters supported");
        
        hash_t res = getAlphaHash(str[0]);
        
        // TODO: with long strings we usually don't need to check for end of string (can avoid unnecessary branching)
        if (res == 0)
            return 0;
        if constexpr (CHARS_IN_HASH == 1)
            return res;

        char hash = getAlphaHash(str[1]);
        if (!hash)
            return (res << 6 * (CHARS_IN_HASH - 1));
        res = (res << 6) + hash;
        if constexpr (CHARS_IN_HASH == 2)
            return res;

        hash = getAlphaHash(str[2]);
        if (!hash)
            return (res << 6 * (CHARS_IN_HASH - 2));
        res = (res << 6) + hash;
        if constexpr (CHARS_IN_HASH == 3)
            return res;

        hash = getAlphaHash(str[3]);
        if (!hash)
            return (res << 6);
        res = (res << 6) + hash;
        return res;
    }

};
