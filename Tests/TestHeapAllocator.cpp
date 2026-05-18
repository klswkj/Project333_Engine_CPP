#include <gtest/gtest.h>
#include "../Source/Core/Public/Memory/Allocator/HeapAllocator.h"

#include <array>
#include <cstdint>

/*
TEST(HeapAllocatorTest, BuildOnly)
{
    SUCCEED(); // This test simply verifies that the HeapAllocator can be instantiated without any issues.
}

TEST(HeapAllocatorTest, AllocateSmallBlock)
{
    HeapAllocator Allocator(1024);
    
    void* Ptr = Allocator.Allocate(16, 8);
    
    ASSERT_NE(Ptr, nullptr);
    
    Allocator.PrintMemoryLayout();
    Allocator.PrintFragmentation();
    ASSERT_TRUE(Allocator.ValidateHeap());
}


TEST(HeapAllocatorTest, AllocateAndFree)
{
    HeapAllocator Allocator(1024);

    void* Ptr = Allocator.Allocate(32, 8);

    ASSERT_NE(Ptr, nullptr);

    Allocator.Deallocate(Ptr);

    Allocator.PrintMemoryLayout();
    Allocator.PrintFragmentation();
    ASSERT_TRUE(Allocator.ValidateHeap());
}

TEST(HeapAllocatorTest, MultipleAllocationsReturnDifferentPointers)
{
    HeapAllocator Allocator(1024);

    void* A = Allocator.Allocate(16, 8);
    void* B = Allocator.Allocate(16, 8);

    ASSERT_NE(A, nullptr);
    ASSERT_NE(B, nullptr);
    ASSERT_NE(A, B);

    Allocator.PrintMemoryLayout();
    Allocator.PrintFragmentation();
    ASSERT_TRUE(Allocator.ValidateHeap());
}

TEST(HeapAllocatorTest, ReturnsProperlyAlignedPointer16)
{
    HeapAllocator Allocator(1024);

    void* Ptr = Allocator.Allocate(64, 16);

    ASSERT_NE(Ptr, nullptr);

    std::uintptr_t Address = reinterpret_cast<std::uintptr_t>(Ptr);
    ASSERT_EQ(Address % 16, 0u);

    Allocator.PrintMemoryLayout();
    Allocator.PrintFragmentation();
    ASSERT_TRUE(Allocator.ValidateHeap());
}

TEST(HeapAllocatorTest, ReusesFreedBlock)
{
    HeapAllocator Allocator(1024);

    void* A = Allocator.Allocate(128, 8);
    ASSERT_NE(A, nullptr);

    Allocator.Deallocate(A);

    void* B = Allocator.Allocate(128, 8);
    ASSERT_NE(B, nullptr);

    Allocator.PrintMemoryLayout();
    Allocator.PrintFragmentation();
    ASSERT_TRUE(Allocator.ValidateHeap());
}

// 여러 번 반복 할당/해제
TEST(HeapAllocatorTest, RepeatedAllocateAndFree)
{
    HeapAllocator Allocator(4096);

    for (int i = 0; i < 100; ++i)
    {
        void* Ptr = Allocator.Allocate(32, 8);
        ASSERT_NE(Ptr, nullptr);
        Allocator.Deallocate(Ptr);
    }

    Allocator.PrintMemoryLayout();
    Allocator.PrintFragmentation();
    ASSERT_TRUE(Allocator.ValidateHeap());
}

// 여러 블록 해제 후 큰 블록 할당
TEST(HeapAllocatorTest, CoalescesFreedBlocks)
{
    HeapAllocator Allocator(2048);

    void* A = Allocator.Allocate(128, 8);
    void* B = Allocator.Allocate(128, 8);
    void* C = Allocator.Allocate(128, 8);

    ASSERT_NE(A, nullptr);
    ASSERT_NE(B, nullptr);
    ASSERT_NE(C, nullptr);

    Allocator.Deallocate(B);
    Allocator.Deallocate(A);

    void* D = Allocator.Allocate(256, 8);
    ASSERT_NE(D, nullptr);
    
    Allocator.PrintMemoryLayout();
    Allocator.PrintFragmentation();
    ASSERT_TRUE(Allocator.ValidateHeap());
}

// 작은 블록 여러개 할당 후 exhaustion 확인
TEST(HeapAllocatorTest, AllocationEventuallyFails)
{
    HeapAllocator Allocator(1024);

    void* Ptrs[64] = {};

    bool bSawNull = false;

    for (int i = 0; i < 64; ++i)
    {
        Ptrs[i] = Allocator.Allocate(32, 8);

        if (nullptr == Ptrs[i])
        {
            bSawNull = true;
            break;
        }
    }

    ASSERT_TRUE(bSawNull);
    Allocator.PrintMemoryLayout();
    Allocator.PrintFragmentation();
    ASSERT_TRUE(Allocator.ValidateHeap());
}

// Deallocate(nullptr) 호출 시 아무 일도 일어나지 않는지 확인
TEST(HeapAllocatorTest, DeallocateNullptrDoesNothing)
{
    HeapAllocator Allocator(1024);

    Allocator.Deallocate(nullptr);

    Allocator.PrintMemoryLayout();
    Allocator.PrintFragmentation();
    ASSERT_TRUE(Allocator.ValidateHeap());
}

// 모든 블록을 할당한 후 모두 해제하면, 
// 전체 힙이 하나의 큰 자유 블록으로 복원되는지 확인
TEST(HeapAllocatorTest, FullFreeRestoresSingleFreeBlock)
{
    HeapAllocator Allocator(1024);

    void* A = Allocator.Allocate(128, 8);
    void* B = Allocator.Allocate(128, 8);
    void* C = Allocator.Allocate(128, 8);

    ASSERT_NE(A, nullptr);
    ASSERT_NE(B, nullptr);
    ASSERT_NE(C, nullptr);

    Allocator.Deallocate(B);
    Allocator.Deallocate(C);
    Allocator.Deallocate(A);

    Allocator.PrintMemoryLayout();
    Allocator.PrintFragmentation();
    ASSERT_TRUE(Allocator.ValidateHeap());
}

// forward/backward coalesce가 둘 다 되는지
TEST(HeapAllocatorTest, CoalescesWithPreviousAndNextBlocks)
{
    HeapAllocator Allocator(2048);

    void* A = Allocator.Allocate(128, 8);
    void* B = Allocator.Allocate(128, 8);
    void* C = Allocator.Allocate(128, 8);

    ASSERT_NE(A, nullptr);
    ASSERT_NE(B, nullptr);
    ASSERT_NE(C, nullptr);

    Allocator.Deallocate(A);
    Allocator.Deallocate(C);
    Allocator.Deallocate(B);

    Allocator.PrintMemoryLayout();
    Allocator.PrintFragmentation();
    ASSERT_TRUE(Allocator.ValidateHeap());
}

// 요청된 정렬이 최소 정렬보다 작은 경우에도 안전하게 처리되는지 확인
TEST(HeapAllocatorTest, AlignmentSmallerThanMinimumIsHandledSafely)
{
    HeapAllocator Allocator(1024);

    void* Ptr = Allocator.Allocate(16, 1);

    ASSERT_NE(Ptr, nullptr);

    std::uintptr_t Address = reinterpret_cast<std::uintptr_t>(Ptr);

    // 현재 구현이 최소 정렬로 보정된다는 전제
    ASSERT_EQ(Address % 8, 0u);

    Allocator.PrintMemoryLayout();
    Allocator.PrintFragmentation();
    ASSERT_TRUE(Allocator.ValidateHeap());
}

// 큰 블록 하나 해제 후 다시 큰 블록이 들어가는지
TEST(HeapAllocatorTest, LargeBlockCanBeReallocatedAfterFree)
{
    HeapAllocator Allocator(2048);

    void* A = Allocator.Allocate(512, 16);
    ASSERT_NE(A, nullptr);

    Allocator.Deallocate(A);

    void* B = Allocator.Allocate(512, 16);
    ASSERT_NE(B, nullptr);

    Allocator.PrintMemoryLayout();
    Allocator.PrintFragmentation();
    ASSERT_TRUE(Allocator.ValidateHeap());
}

#if RB_TREE_DEBUG
TEST(HeapAllocatorTest, StatsCountSuccessfulAllocateAndDeallocate)
{
    Core::HeapAllocator Allocator(1024);

    void* A = Allocator.Allocate(16, 8);
    void* B = Allocator.Allocate(32, 8);

    ASSERT_NE(A, nullptr);
    ASSERT_NE(B, nullptr);

    // A만 해제한 상태의 통계를 검증한다.
    Allocator.Deallocate(A);

    // 호출 시도:
    // Allocate 2번 + Deallocate 1번 = 3
    ASSERT_EQ(Allocator.GetTotalRequestCount(), 3u);

    // 성공한 Allocate 2번
    ASSERT_EQ(Allocator.GetTotalAllocationCount(), 2u);

    // 성공한 Deallocate 1번
    ASSERT_EQ(Allocator.GetTotalDeallocationCount(), 1u);

    // 요청 크기 누적:
    // Allocate(16) + Allocate(32) = 48
    ASSERT_EQ(Allocator.GetAccumulatedRequestSize(), 48u);

    // 해제 크기 누적:
    // A는 16 요청 + 64 헤더 = BlockSize 80
    ASSERT_EQ(Allocator.GetAccumulatedFreeSize(), 80u);

    ASSERT_TRUE(Allocator.ValidateHeap());

    // 테스트 종료 전에 남은 B도 해제해야 한다.
    // HeapAllocator 소멸자에서 아직 사용 중인 블록이 있으면 memory leak으로 CHECKF가 터진다.
    Allocator.Deallocate(B);

    // B까지 해제한 뒤 최종 통계도 확인한다.
    // Allocate 2번 + Deallocate 2번 = 4
    ASSERT_EQ(Allocator.GetTotalRequestCount(), 4u);

    // 성공한 Allocate 2번
    ASSERT_EQ(Allocator.GetTotalAllocationCount(), 2u);

    // 성공한 Deallocate 2번
    ASSERT_EQ(Allocator.GetTotalDeallocationCount(), 2u);

    // 요청 크기 누적은 Allocate 요청만 누적하므로 그대로 48
    ASSERT_EQ(Allocator.GetAccumulatedRequestSize(), 48u);

    // 해제 크기 누적:
    // A: 16 + 64 = 80
    // B: 32 + 64 = 96
    // 총 176
    ASSERT_EQ(Allocator.GetAccumulatedFreeSize(), 176u);

    ASSERT_TRUE(Allocator.ValidateHeap());
}

TEST(HeapAllocatorTest, StatsIncludeFailedAllocateRequestButNotSuccessfulAllocationCount)
{
    Core::HeapAllocator Allocator(128);

    void* A = Allocator.Allocate(16, 8);
    ASSERT_NE(A, nullptr);

    void* B = Allocator.Allocate(1024, 8);
    ASSERT_EQ(B, nullptr);

    // 호출 시도:
    // Allocate 2번
    ASSERT_EQ(Allocator.GetTotalRequestCount(), 2u);

    // 성공한 Allocate는 첫 번째만
    ASSERT_EQ(Allocator.GetTotalAllocationCount(), 1u);

    // Deallocate 성공은 아직 없음
    ASSERT_EQ(Allocator.GetTotalDeallocationCount(), 0u);

    // 요청 크기 누적은 성공/실패와 무관하게 Allocate 입력값 누적
    // 16 + 1024
    ASSERT_EQ(Allocator.GetAccumulatedRequestSize(), 1040u);

    // free된 블록은 없음
    ASSERT_EQ(Allocator.GetAccumulatedFreeSize(), 0u);

    Allocator.Deallocate(B);
    Allocator.Deallocate(A);

    ASSERT_TRUE(Allocator.ValidateHeap());
}

TEST(HeapAllocatorTest, StatsCountNullDeallocateAsRequestButNotAsSuccessfulDeallocation)
{
    Core::HeapAllocator Allocator(1024);

    void* A = Allocator.Allocate(16, 8);
    ASSERT_NE(A, nullptr);

    Allocator.Deallocate(nullptr);
    Allocator.Deallocate(A);

    // 호출 시도:
    // Allocate 1번 + Deallocate(nullptr) 1번 + Deallocate(A) 1번 = 3
    ASSERT_EQ(Allocator.GetTotalRequestCount(), 3u);

    // 성공한 Allocate 1번
    ASSERT_EQ(Allocator.GetTotalAllocationCount(), 1u);

    // 성공한 Deallocate는 A에 대한 1번만
    ASSERT_EQ(Allocator.GetTotalDeallocationCount(), 1u);

    // 요청 크기 누적: 16
    ASSERT_EQ(Allocator.GetAccumulatedRequestSize(), 16u);

    // A free 누적: 16 요청 + 64 헤더 = 80
    ASSERT_EQ(Allocator.GetAccumulatedFreeSize(), 80u);

    ASSERT_TRUE(Allocator.ValidateHeap());
}

#endif
*/

TEST(HeapAllocatorTest, ReallocateNullPointerActsLikeAllocate)
{
    Core::HeapAllocator Allocator(1024);

    void* Ptr = Allocator.Reallocate(nullptr, 32, 8);

    ASSERT_NE(Ptr, nullptr);

    Allocator.Deallocate(Ptr);

    ASSERT_TRUE(Allocator.ValidateHeap());
}

TEST(HeapAllocatorTest, ReallocateZeroSizeActsLikeDeallocate)
{
    Core::HeapAllocator Allocator(1024);

    void* Ptr = Allocator.Allocate(32, 8);

    ASSERT_NE(Ptr, nullptr);

    void* NewPtr = Allocator.Reallocate(Ptr, 0, 8);

    ASSERT_EQ(NewPtr, nullptr);
    ASSERT_TRUE(Allocator.ValidateHeap());
}

TEST(HeapAllocatorTest, ReallocateShrinkKeepsSamePointerAndData)
{
    Core::HeapAllocator Allocator(1024);

    char* Ptr = static_cast<char*>(Allocator.Allocate(128, 8));

    ASSERT_NE(Ptr, nullptr);

    Ptr[0] = 10;
    Ptr[31] = 20;
    Ptr[63] = 30;
    Ptr[127] = 40;

    void* NewPtr = Allocator.Reallocate(Ptr, 32, 8);

    ASSERT_EQ(NewPtr, Ptr);

    char* NewCharPtr = static_cast<char*>(NewPtr);

    EXPECT_EQ(10, NewCharPtr[0]);
    EXPECT_EQ(20, NewCharPtr[31]);

    Allocator.Deallocate(NewPtr);

    ASSERT_TRUE(Allocator.ValidateHeap());
}

TEST(HeapAllocatorTest, ReallocateExpandInPlaceWhenNextBlockIsFree)
{
    Core::HeapAllocator Allocator(2048);

    char* A = static_cast<char*>(Allocator.Allocate(64, 8));
    void* B = Allocator.Allocate(256, 8);

    ASSERT_NE(A, nullptr);
    ASSERT_NE(B, nullptr);

    A[0] = 11;
    A[31] = 22;
    A[63] = 33;

    Allocator.Deallocate(B);

    void* NewA = Allocator.Reallocate(A, 256, 8);

    ASSERT_EQ(NewA, A);

    char* NewCharA = static_cast<char*>(NewA);

    EXPECT_EQ(11, NewCharA[0]);
    EXPECT_EQ(22, NewCharA[31]);
    EXPECT_EQ(33, NewCharA[63]);

    Allocator.Deallocate(NewA);

    ASSERT_TRUE(Allocator.ValidateHeap());
}

TEST(HeapAllocatorTest, ReallocateExpandAllocatesNewBlockWhenNextBlockIsUsed)
{
    Core::HeapAllocator Allocator(4096);

    char* A = static_cast<char*>(Allocator.Allocate(64, 8));
    void* B = Allocator.Allocate(256, 8);

    ASSERT_NE(A, nullptr);
    ASSERT_NE(B, nullptr);

    A[0] = 44;
    A[31] = 55;
    A[63] = 66;

    void* NewA = Allocator.Reallocate(A, 512, 8);

    ASSERT_NE(NewA, nullptr);
    ASSERT_NE(NewA, A);

    char* NewCharA = static_cast<char*>(NewA);

    EXPECT_EQ(44, NewCharA[0]);
    EXPECT_EQ(55, NewCharA[31]);
    EXPECT_EQ(66, NewCharA[63]);

    Allocator.Deallocate(NewA);
    Allocator.Deallocate(B);

    ASSERT_TRUE(Allocator.ValidateHeap());
}

TEST(HeapAllocatorTest, ReallocatePreservesOnlyMinOldAndNewPayloadSize)
{
    Core::HeapAllocator Allocator(1024);

    char* Ptr = static_cast<char*>(Allocator.Allocate(64, 8));

    ASSERT_NE(Ptr, nullptr);

    for (size_t i = 0; i < 64; ++i)
    {
        Ptr[i] = static_cast<char>(i + 1);
    }

    void* NewPtr = Allocator.Reallocate(Ptr, 16, 8);

    ASSERT_EQ(NewPtr, Ptr);

    char* NewCharPtr = static_cast<char*>(NewPtr);

    for (size_t i = 0; i < 16; ++i)
    {
        EXPECT_EQ(static_cast<char>(i + 1), NewCharPtr[i]);
    }

    Allocator.Deallocate(NewPtr);

    ASSERT_TRUE(Allocator.ValidateHeap());
}

TEST(HeapAllocatorTest, DeallocateCoalescesWithNextFreeBlock)
{
    Core::HeapAllocator Allocator(2048);

    void* A = Allocator.Allocate(64, 8);
    void* B = Allocator.Allocate(64, 8);

    ASSERT_NE(A, nullptr);
    ASSERT_NE(B, nullptr);

    Allocator.Deallocate(B);
    Allocator.Deallocate(A);

    ASSERT_TRUE(Allocator.ValidateHeap());

    void* Large = Allocator.Allocate(256, 8);

    ASSERT_NE(Large, nullptr);

    Allocator.Deallocate(Large);

    ASSERT_TRUE(Allocator.ValidateHeap());
}

TEST(HeapAllocatorTest, DeallocateCoalescesWithPreviousFreeBlock)
{
    Core::HeapAllocator Allocator(2048);

    void* A = Allocator.Allocate(64, 8);
    void* B = Allocator.Allocate(64, 8);

    ASSERT_NE(A, nullptr);
    ASSERT_NE(B, nullptr);

    Allocator.Deallocate(A);
    Allocator.Deallocate(B);

    ASSERT_TRUE(Allocator.ValidateHeap());

    void* Large = Allocator.Allocate(256, 8);

    ASSERT_NE(Large, nullptr);

    Allocator.Deallocate(Large);

    ASSERT_TRUE(Allocator.ValidateHeap());
}

TEST(HeapAllocatorTest, DeallocateCoalescesWithPreviousAndNextFreeBlocks)
{
    Core::HeapAllocator Allocator(4096);

    void* A = Allocator.Allocate(64, 8);
    void* B = Allocator.Allocate(64, 8);
    void* C = Allocator.Allocate(64, 8);

    ASSERT_NE(A, nullptr);
    ASSERT_NE(B, nullptr);
    ASSERT_NE(C, nullptr);

    Allocator.Deallocate(A);
    Allocator.Deallocate(C);

    ASSERT_TRUE(Allocator.ValidateHeap());

    Allocator.Deallocate(B);

    ASSERT_TRUE(Allocator.ValidateHeap());

    void* Large = Allocator.Allocate(512, 8);

    ASSERT_NE(Large, nullptr);

    Allocator.Deallocate(Large);

    ASSERT_TRUE(Allocator.ValidateHeap());
}

// Allocate/Reallocate 포인트 alignment 테스트
TEST(HeapAllocatorTest, AllocateReturnsAlignedPointers)
{
    Core::HeapAllocator Allocator(4096);

    void* Ptr8  = Allocator.Allocate(24, 8);
    void* Ptr16 = Allocator.Allocate(24, 16);

    ASSERT_NE(Ptr8, nullptr);
    ASSERT_NE(Ptr16, nullptr);

    EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(Ptr8)  % 8);
    EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(Ptr16) % 16);

    Allocator.Deallocate(Ptr8);
    Allocator.Deallocate(Ptr16);

    ASSERT_TRUE(Allocator.ValidateHeap());
}

TEST(HeapAllocatorTest, ReallocateReturnsAlignedPointer)
{
    Core::HeapAllocator Allocator(4096);

    void* Ptr = Allocator.Allocate(24, 8);

    ASSERT_NE(Ptr, nullptr);

    void* NewPtr = Allocator.Reallocate(Ptr, 128, 16);

    ASSERT_NE(NewPtr, nullptr);

    EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(NewPtr) % 16);

    Allocator.Deallocate(NewPtr);

    ASSERT_TRUE(Allocator.ValidateHeap());
}

// ------------------------------------------------------------------
// Reallocate expand in-place 후 남은 split free block 재사용 테스트
// ------------------------------------------------------------------
TEST(HeapAllocatorTest, ReallocateExpandInPlaceLeavesReusableSplitFreeBlock)
{
    Core::HeapAllocator Allocator(4096);

    char* A = static_cast<char*>(Allocator.Allocate(64, 8));
    void* B = Allocator.Allocate(512, 8);

    ASSERT_NE(A, nullptr);
    ASSERT_NE(B, nullptr);

    A[0] = 77;
    A[63] = 88;

    Allocator.Deallocate(B);

    void* NewA = Allocator.Reallocate(A, 256, 8);

    ASSERT_EQ(A, NewA);

    char* NewCharA = static_cast<char*>(NewA);

    EXPECT_EQ(77, NewCharA[0]);
    EXPECT_EQ(88, NewCharA[63]);

    // A가 256 payload로 커진 뒤, 남은 공간이 split되어 free block으로 재등록되었는지 확인.
    void* Reused = Allocator.Allocate(128, 8);

    ASSERT_NE(Reused, nullptr);

    Allocator.Deallocate(Reused);
    Allocator.Deallocate(NewA);

    ASSERT_TRUE(Allocator.ValidateHeap());
}


// ---------------------------------------------
// 스트레스 테스트
// ---------------------------------------------

TEST(HeapAllocatorTest, StressAllocateDeallocateReallocate)
{
    Core::HeapAllocator Allocator(64 * 1024);

    constexpr size_t PointerCount = 128;

    void* Pointers[PointerCount] = {};

    for (size_t i = 0; i < PointerCount; ++i)
    {
        const size_t Size = 8 + ((i * 13) % 128);
        const size_t Alignment = (0 == (i % 2)) ? 8 : 16;

        Pointers[i] = Allocator.Allocate(Size, Alignment);

        ASSERT_NE(Pointers[i], nullptr);
        EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(Pointers[i]) % Alignment);
    }

    for (size_t i = 0; i < PointerCount; i += 2)
    {
        Allocator.Deallocate(Pointers[i]);
        Pointers[i] = nullptr;
    }

    for (size_t i = 1; i < PointerCount; i += 4)
    {
        const size_t NewSize = 256 + ((i * 7) % 256);

        void* NewPtr = Allocator.Reallocate(Pointers[i], NewSize, 16);

        ASSERT_NE(NewPtr, nullptr);
        EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(NewPtr) % 16);

        Pointers[i] = NewPtr;
    }

    for (size_t i = 0; i < PointerCount; ++i)
    {
        if (nullptr != Pointers[i])
        {
            Allocator.Deallocate(Pointers[i]);
            Pointers[i] = nullptr;
        }
    }

    ASSERT_TRUE(Allocator.ValidateHeap());
}