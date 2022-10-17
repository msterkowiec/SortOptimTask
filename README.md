[![CMake-GCC](https://github.com/msterkowiec/SortOptimTask/actions/workflows/cmake-gcc.yaml/badge.svg?branch=main)](https://github.com/msterkowiec/SortOptimTask/actions/workflows/cmake-gcc.yaml)

SortOptimTask

An initial solution that is already outperforming multithreaded std::sort.

Complexity is O(n log(n))  (log with base 2) although multithreading makes is slightly better, like O(n log(n/t))

Similarly using a hash map (with m elements) reduces computational complexity to O(m n/m log(n/m)) which is O(n log(n/m))

Because m > t (practically even  m >> t; and we also make use of multithreading) we may expect quite significant gain over multithreaded std::sort.
However the initial version outperforms it now much, 10-20% - anyway the more elements, the higher gain, for example (local run; MSVC) :

    Test with string length 792723456 has just started...
    Adding elements to hash_map (single thread) took 66350 millisecond(s)
    Multithreaded sort with hash map took 90545 millisecond(s)  <--------
    Adding elements to array took 17136 millisecond(s)
    Simple multithreaded sort took 120969 millisecond(s) <-------- (note: gcc C++ version on github doesn't allow for mutithreaded std::sort yet)
    Full match
    
So far either profiling (did it a few times) nor heuristical approach (pausing app at random places) did not help much.
Rather thinking why this algorithm only slightly outperforms multithreaded std::sort, gives hints where to look for enhancements.

Now it looks like this first long single-threaded phase ("Adding elements to hash_map (single thread) took 66350 millisecond(s)") requires change - I intend to split it into two multithreaded parts:
1) calculate all posssible hashes and get their counters (e.g. memory allocation will then be much easier)
2) add them to hash map - also in a multithreaded way
