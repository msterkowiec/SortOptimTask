std::string CreateRandomString(size_t len)
{
    std::mt19937 mt;
    static const size_t NUMCHARS = ('z' - 'a' + 1) * 2;
    std::string s;
    s.resize(len);
    s.c_str();
    size_t rand;
    for (size_t i = 0; i < len; ++i)
    {
        rand = mt() % NUMCHARS;
        if (rand < NUMCHARS / 2)
            s[i] = 'a' + rand;
        else
            s[i] = 'A' + rand - NUMCHARS / 2;
    }
    return s; // RVO
}

void Test(const std::string& s)
{
    try
    {
        std::cout << "Test with string length " << s.size() << " has just started...\n";

        // Sort (hash map) :
        auto start = std::chrono::steady_clock::now();
        std::unique_ptr<HashMap> hashMap(new HashMap(s.size()));
        for (size_t i = 0; i < s.size(); ++i)
            hashMap->add(&s[i]);
        std::cout << "Adding elements to hash_map (single thread) took " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() << " millisecond(s)\n";
        hashMap->sort();
        std::cout << "Multithreaded sort with hash map took " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() << " millisecond(s)\n";

        // Sort (simple) :
        start = std::chrono::steady_clock::now();
        std::vector<const char*> vecPtr(s.size());
        for (size_t i = 0; i < s.size(); ++i)
            vecPtr[i] = &s[i];
        std::cout << "Adding elements to array took " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() << " millisecond(s)\n";

        std::sort(std::execution::par_unseq, vecPtr.begin(), vecPtr.end(), CStrComparator());

        std::cout << "Simple multithreaded sort took " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() << " millisecond(s)\n";

        // Compare results:

        auto ret = hashMap->getFirst();
        if (vecPtr[0] != ret.first)
        {
            std::cout << "Inconsistency at index 0\n";
            EXPECT_FALSE(true); 
            return;
        }
        for (size_t i = 1; i < LEN; ++i)
        {
            ret = hashMap->getNext(ret.second);
            if (vecPtr[i] != ret.first)
            {
                std::cout << "Inconsistency at index " << i << "\n";
                EXPECT_FALSE(true); 
                return;
            }
        }
        std::cout << "Full match\n";
    }
    catch (std::bad_alloc& ba)
    {
        std::cout << "Not enough memory\n";
        EXPECT_FALSE(true); 
    }
    catch (...)
    {
        std::cout << "Exception caught\n";
        EXPECT_FALSE(true); 
    }  
}

TEST(SortOptimTask, Test1MB)
{
   auto s = CreateRandomString(1024 * 1024);
  
   Test(s);  
}

TEST(SortOptimTask, Test16MB)
{
   auto s = CreateRandomString(16 * 1024 * 1024);
  
   Test(s);  
}
