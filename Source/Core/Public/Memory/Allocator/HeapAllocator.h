#pragma once

#include "IAllocator.h"

#include "../MallocBase.h"

#define RB_TREE_DEBUG_ALLOW 1

#if defined(_DEBUG) && RB_TREE_DEBUG_ALLOW
	#define RB_TREE_DEBUG 1
#else
	#define RB_TREE_DEBUG 0
#endif

#if RB_TREE_DEBUG
	#include<assert.h>
	#include<stdint.h> // For uint32_t
#endif

// HeapAllocator policy:
// BlockSize means the total physical block size.
// BlockSize = sizeof(BlockHeader) + PayloadSize.
//
// Alignment policy:
// HeapAllocator currently guarantees up to 16-byte alignment.

namespace Core
{
#if RB_TREE_DEBUG
	enum class ECanary : uint32_t
	{
		Free = 0xDEADBEEF,
		Used = 0xDEADC0DE,
		// 0xFEEDFACE
		// 0xBAADF00D
		// 0xCAFEBABE
		// 0xBADC0FFE
		// 0x0BADC0DE
		// 0xABADBABE
	};
#endif

	// #pragma pack(push, 1) // Ensure no padding is added to the struct
	// #if RB_TREE_DEBUG
	//	ArrSize : 56bytes, Align : 8bytes
	// #else
	//	ArrSize  : 32bytes
	//	Align : 4bytes
	// #endif
	struct alignas(16) BlockHeader final
	{
		size_t BlockSize;

		// Physical block info (doubly linked list)
		BlockHeader* NextPhysBlock;
		BlockHeader* PrevPhysBlock;

		// RB-tree (intrusive) 
		BlockHeader* Left;
		BlockHeader* Right;
		BlockHeader* Parent;

		bool   IsFree;
		bool   IsRed;

#if RB_TREE_DEBUG
		ECanary CanaryValue; // For debugging purposes, to detect memory corruption
#endif
	};

	extern BlockHeader NilNode;

	struct NilInitializer final
	{
		NilInitializer()
		{
			NilNode = {};
			NilNode.IsRed = false;

			NilNode.Left = &NilNode;
			NilNode.Right = &NilNode;
			NilNode.Parent = &NilNode;
		}
	};

	static NilInitializer NilInit;

	struct RBTree final
	{
		BlockHeader* Root;
		BlockHeader* Nil = nullptr;

		// static BlockHeader NilNode;

		RBTree()
		{
			Nil = &NilNode;
			Nil->IsRed = false;

			Nil->Left = Nil;
			Nil->Right = Nil;
			Nil->Parent = Nil;

			Root = Nil;
		}

		void Insert(BlockHeader* InputNode);
		void Remove(BlockHeader* InputNode);

		BlockHeader* FindBestFit(size_t InputSize) const;

	private:
		void RotateLeft(BlockHeader* InputNode);
		void RotateRight(BlockHeader* InputNode);

		void FixInsert(BlockHeader* InputNode);
		void FixRemove(BlockHeader* InputNode);

		void Transplant(BlockHeader* U, BlockHeader* V);
		BlockHeader* Minimum(BlockHeader* InputNode) const;
	};

	// HeapAllocator
	//  ㄴ RBTree (Best-Fit)
	//    ㄴ BlockHeader (intrusive RB-tree node + physical block info)

	class HeapAllocator final : public IMallocBase
	{
	private:
		void* HeapStart;
		size_t TotalHeapSize;
		RBTree FreeBlocksTree;

#if RB_TREE_DEBUG
		// 1단계
		size_t m_UsedBlockCount;
		size_t m_FreeBlockCount;
		size_t TotalFreeBytes;

		// 2단계
		size_t AccumulatedRequestSize;
		size_t AccumulatedFreeSize;
		size_t TotalAllocationCount;
		size_t TotalDeallocationCount;
		size_t TotalReallocationCount;
		size_t TotalRequestCount;

		// 3단계
		size_t PeakUsedBlockCount;
		size_t PeakUsedBytes;
		size_t PeakAllocationCount;
		size_t PeakDeallocationCount;
		size_t PeakRequestCount;
		size_t PeakFragmentation; // (1 - (LargestFreeBlockSize / TotalFreeBytes)) * 100
		size_t PeakExternalFragmentation; // (1 - (LargestFreeBlockSize / TotalHeapSize)) * 100
		size_t PeakInternalFragmentation; // (AccumulatedFreeSize / AccumulatedRequestSize) * 100

		// 4단계
		size_t PeakTotalFragmentation; // (1 - (LargestFreeBlockSize / TotalHeapSize)) * 100
		size_t PeakFragmentationAtAllocationFailure; // Fragmentation when an allocation fails due to fragmentation
		size_t PeakExternalFragmentationAtAllocationFailure; // External fragmentation when an allocation fails due to fragmentation
		size_t PeakInternalFragmentationAtAllocationFailure; // Internal fragmentation when an allocation fails due to fragmentation
		size_t PeakTotalFragmentationAtAllocationFailure; // Total fragmentation when an allocation fails due to fragmentation

		size_t TotalFailedAllocationCount;
#endif

	public:
		HeapAllocator(size_t InputTotalHeapSize);
		virtual ~HeapAllocator();

		virtual void* Allocate(size_t InputRequestSize, size_t InputAlignment) override;
		virtual void* Reallocate(void* InputPtr, size_t InputNewSize, size_t InputAlignment) override;
		
		virtual void  Deallocate(void* InputPtr) override;

#if RB_TREE_DEBUG
		bool ValidateHeap()       const;
		void PrintMemoryLayout()  const;
		void PrintFragmentation() const;

		size_t GetFreeBlockCount() const { return m_FreeBlockCount; }
		size_t GetUsedBlockCount() const { return m_UsedBlockCount; }
		size_t GetTotalFreeBytes() const { return TotalFreeBytes; }
		// size_t GetLargestFreeBlockSize()  const { return m_LargestFreeBlockSize; }
		// size_t GetSmallestFreeBlockSize() const { return m_SmallestFreeBlockSize; }

		size_t GetTotalRequestCount()          const { return TotalRequestCount; }
		size_t GetTotalAllocationCount()       const { return TotalAllocationCount; }
		size_t GetTotalDeallocationCount()     const { return TotalDeallocationCount; }
		size_t GetTotalFailedAllocationCount() const { return TotalFailedAllocationCount; }

		size_t GetAccumulatedRequestSize()     const { return AccumulatedRequestSize; }
		size_t GetAccumulatedFreeSize()        const { return AccumulatedFreeSize; }

		// @todo : 화면상의 메모리 레이아웃을 시각화해서 화면에 띄워주는 것 만들기
#endif
	private:
		BlockHeader* SplitUsedBlock(BlockHeader* InputBlock, size_t InputUsedPayloadSize);
	};
}