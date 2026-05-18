include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(TestHeapAllocator
    TestHeapAllocator.cpp
    TestVector.cpp
    TestArray.cpp
    TestQueue.cpp
    TestStack.cpp
    TestLockFreeQueue.cpp
    TestFixedSlotPool.cpp)

target_link_libraries(TestHeapAllocator
    PRIVATE
    Core
    GTest::gtest
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(TestHeapAllocator)
