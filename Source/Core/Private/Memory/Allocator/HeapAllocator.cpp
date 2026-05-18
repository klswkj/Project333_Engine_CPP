// #include "Memory/HeapAllocator.h"
#include "../../../Public/Memory/Allocator/HeapAllocator.h"
#include "../../../Public/CoreMinimal.h"
#include "../../../Public/Memory/MemoryAlignmentPolicy.h"

#include <stdlib.h>
#include <stdio.h>

namespace MAP = Core::MemoryAlignmentPolicy;

namespace Core
{
	BlockHeader NilNode;
}

#pragma region RBTree Implementation

void Core::RBTree::Insert(BlockHeader* InputNode)
{
	BlockHeader* X = Root; // CurrentNode
	BlockHeader* Y = Nil;  // ParentNode

	while (Nil != X)
	{
		Y = X;

		if (InputNode->BlockSize < X->BlockSize)
		{
			X = X->Left;
		}
		else
		{
			X = X->Right;
		}
	}

	InputNode->Parent = Y;

	if (Nil == Y)
	{
		Root = InputNode;
	}
	else if (InputNode->BlockSize < Y->BlockSize)
	{
		Y->Left = InputNode;
	}
	else
	{
		Y->Right = InputNode;
	}

	InputNode->Left = Nil;
	InputNode->Right = Nil;
	InputNode->IsRed = true;

	FixInsert(InputNode);
}

void Core::RBTree::FixInsert(BlockHeader* InputNode)
{
	// Must NilNode->IsRed == false, so this loop will terminate
	while (InputNode->Parent->IsRed)
	{
		BlockHeader* Parent      = InputNode->Parent;
		BlockHeader* GrandParent = Parent->Parent;
		
		bool IsParentLeftChild = (Parent == GrandParent->Left);
		BlockHeader* Uncle = (IsParentLeftChild) ? GrandParent->Right : GrandParent->Left;

		if (IsParentLeftChild)
		{
			if (Uncle->IsRed)
			{
				Parent->IsRed      = false;
				Uncle->IsRed       = false;
				GrandParent->IsRed = true;

				InputNode = GrandParent;
			}
			else // If Uncle is black
			{
				if (InputNode == Parent->Right)
				{
					InputNode = Parent;
					RotateLeft(InputNode);
				}

				InputNode->Parent->IsRed         = false;
				InputNode->Parent->Parent->IsRed = true;

				RotateRight(InputNode->Parent->Parent);
			}
		}
		else // If Parent is a right child
		{
			if (Uncle->IsRed)
			{
				Parent->IsRed = false;
				Uncle->IsRed = false;
				GrandParent->IsRed = true;
				InputNode = GrandParent;
			}
			else // If Uncle is black
			{
				if(InputNode == Parent->Left)
				{
					InputNode = Parent;
					RotateRight(InputNode);
				}

				InputNode->Parent->IsRed         = false;
				InputNode->Parent->Parent->IsRed = true;

				RotateLeft(InputNode->Parent->Parent);
			}
		} // End of if (IsParentLeftChild)
	} // End of while (TargetNode->Parent->IsRed)

	Root->IsRed = false;
}

void Core::RBTree::Remove(BlockHeader* TargetNode)
{
	BlockHeader* NodeToRemove = TargetNode; // Node to be removed
	BlockHeader* NodeToFix    = nullptr;    // Node to be fixed

	bool bRemovedNodeWasRed = NodeToRemove->IsRed; // (Fixup is only needed if the removed node is black)

	if (Nil == TargetNode->Left) // TargetNode has no left child
	{
		// we can simply replace TargetNode with its right child (which may be Nil)
		NodeToFix = TargetNode->Right;
		Transplant(TargetNode, TargetNode->Right);
	}
	else if (Nil == TargetNode->Right) // TargetNode has no right child
	{
		// we can simply replace TargetNode with its left child (which may be Nil)
		NodeToFix = TargetNode->Left;
		Transplant(TargetNode, TargetNode->Left);
	}
	else // TargetNode has two children
	{
		// NodeToRemove is the actual node that will be physically removed from the tree
		// (it is either TargetNode itself or its successor)

		NodeToRemove = Minimum(TargetNode->Right); 
		bRemovedNodeWasRed = NodeToRemove->IsRed;
		NodeToFix          = NodeToRemove->Right;

		if (NodeToRemove->Parent == TargetNode) // If NodeToRemove is a direct child of TargetNode
		{
			// Nil's parent, left, and right are all set to itself, 
			// so if NodeToRemove->Right is Nil, it will still work correctly
			NodeToFix->Parent = NodeToRemove;
		}
		else // If NodeToRemove is more deep in the right subtree
		{
			Transplant(NodeToRemove, NodeToRemove->Right);

			NodeToRemove->Right         = TargetNode->Right;
			NodeToRemove->Right->Parent = NodeToRemove;
		}

		Transplant(TargetNode, NodeToRemove);
		NodeToRemove->Left  = TargetNode->Left;
		NodeToRemove->Left->Parent = NodeToRemove;
		NodeToRemove->IsRed = TargetNode->IsRed;
	}
	if (!bRemovedNodeWasRed)
	{
		FixRemove(NodeToFix);
	}
}

void Core::RBTree::Transplant(BlockHeader* U, BlockHeader* V)
{
	if (Nil == U->Parent) // U is the root
	{
		Root = V;
	}
	else if (U == U->Parent->Left) // U is a left child
	{
		U->Parent->Left = V;
	}
	else // U is a right child
	{
		U->Parent->Right = V;
	}

	V->Parent = U->Parent;
}

void Core::RBTree::FixRemove(BlockHeader* InputNode)
{
	// X is the node that replaces the removed node (NodeToFix in Remove function)

	// Condition : 
	// 1. InputNode is not the root (because if it is the root, we can simply recolor it to black and exit)
	// 2. InputNode is black (because if it is red, we can simply recolor it to black and exit)
	while ( InputNode != Root
		&& !InputNode->IsRed)
	{
		if (InputNode == InputNode->Parent->Left) // If InputNode is a left child
		{
			BlockHeader* Sibling = InputNode->Parent->Right;

			if (Sibling->IsRed) // Case 1: Sibling is red
			{
				Sibling->IsRed = false;
				InputNode->Parent->IsRed = true;

				RotateLeft(InputNode->Parent);
				Sibling = InputNode->Parent->Right; // Update sibling after rotation
			}

			// After Case 1, sibling is black, 
			// silbling's children are both black, 
			// or sibling's right child is red
			if (!Sibling->Left->IsRed && !Sibling->Right->IsRed) // Case 2: Sibling is black and both of sibling's children are black
			{
				Sibling->IsRed = true;
				InputNode = InputNode->Parent; // Move up the tree
			}
			else // Case 3 and Case 4: Sibling is black and at least one of sibling's children is red
			{
				// Case 3: Sibling's right child is black, 
				// so sibling's left child must be red
				if (!Sibling->Right->IsRed)
				{
					Sibling->Left->IsRed = false;
					Sibling->IsRed = true;
					RotateRight(Sibling);
					Sibling = InputNode->Parent->Right; // Update sibling after rotation
				}

				// Case 4: Sibling's right child is red
				Sibling->IsRed = InputNode->Parent->IsRed;
				InputNode->Parent->IsRed = false;
				Sibling->Right->IsRed = false;
				RotateLeft(InputNode->Parent);

				InputNode = Root; // Exit the loop
			}
		}
		else // If InputNode is a right child (mirror case)
		{
			BlockHeader* Sibling = InputNode->Parent->Left;
			if (Sibling->IsRed) // Case 1: Sibling is red
			{
				Sibling->IsRed = false;
				InputNode->Parent->IsRed = true;
				RotateRight(InputNode->Parent);
				Sibling = InputNode->Parent->Left; // Update sibling after rotation
			}
			if (!Sibling->Left->IsRed && !Sibling->Right->IsRed) // Case 2: Sibling is black and both of sibling's children are black
			{
				Sibling->IsRed = true;
				InputNode = InputNode->Parent; // Move up the tree
			}
			else // Case 3 and Case 4: Sibling is black and at least one of sibling's children is red
			{
				if (!Sibling->Left->IsRed) // Case 3: Sibling's left child is black, so sibling's right child must be red
				{
					Sibling->Right->IsRed = false;
					Sibling->IsRed = true;
					RotateLeft(Sibling);
					Sibling = InputNode->Parent->Left; // Update sibling after rotation
				}
				// Case 4: Sibling's left child is red
				Sibling->IsRed = InputNode->Parent->IsRed;
				InputNode->Parent->IsRed = false;
				Sibling->Left->IsRed = false;
				RotateRight(InputNode->Parent);
				InputNode = Root; // Exit the loop
			}
		}
	}

	// Fix the double-black by turning InputNode into black
	InputNode->IsRed = false;
}

Core::BlockHeader* Core::RBTree::Minimum(BlockHeader* InputNode) const
{
	while (Nil != InputNode->Left)
	{
		InputNode = InputNode->Left;
	}

	return InputNode;
}

Core::BlockHeader* Core::RBTree::FindBestFit(size_t InputRequestSize) const
{
	BlockHeader* CurrentNode = Root;
	BlockHeader* BestFitNode = Nil;

	while (Nil != CurrentNode)
	{
#if RB_TREE_DEBUG
		CHECKF(CurrentNode->IsFree, L"Found a non-free block while searching for best fit", 1);
#endif

		if (InputRequestSize <= CurrentNode->BlockSize)
		{
			BestFitNode = CurrentNode;
			CurrentNode = CurrentNode->Left;
		}
		else
		{
			CurrentNode = CurrentNode->Right;
		}
	}

	return (BestFitNode == Nil) ? nullptr : BestFitNode;
}

void Core::RBTree::RotateLeft(BlockHeader* X)
{
	BlockHeader* Y = X->Right;

	X->Right = Y->Left;

	if (RBTree::Nil != Y->Left)
	{
		Y->Left->Parent = X;
	}

	Y->Parent = X->Parent;
	
	if (RBTree::Nil == X->Parent)
	{
		Root = Y;
	}
	else if (X == X->Parent->Left)
	{
		X->Parent->Left = Y;
	}
	else
	{
		X->Parent->Right = Y;
	}

	Y->Left   = X;
	X->Parent = Y;
}

void Core::RBTree::RotateRight(BlockHeader* X)
{
	BlockHeader* Y = X->Left;

	X->Left = Y->Right;

	if (Nil != Y->Right)
	{
		Y->Right->Parent = X;
	}

	Y->Parent = X->Parent;

	if (Nil == X->Parent)
	{
		Root = Y;
	}
	else if (X == X->Parent->Right)
	{
		X->Parent->Right = Y;
	}
	else
	{
		X->Parent->Left = Y;
	}

	Y->Right  = X;
	X->Parent = Y;
}

#pragma endregion RBTree Implementation

#pragma region HeapAllocator Implementation

// 코드설계 핵심 공식
// const size_t PayloadSize = Block->BlockSize - sizeof(BlockHeader);
// const size_t RequiredPayloadSize = MAP::AlignUP(InputRequestSize, InputAlignment);
// const size_t RequiredBlockSize   = sizeof(BlockHeader) + RequiredPayloadSize;
//
// Coalesce시 :
// PrevBlock->BlockSize += BlockToFree->BlockSize;
// BlockToFree->BlockSize += NextBlock->BlockSize;
//
// const size_t CombinedBlockSize = OldBlock->BlockSize + NextBlock->BlockSize;

Core::HeapAllocator::HeapAllocator(size_t InTotalSize)
	: TotalHeapSize(InTotalSize)
	#if RB_TREE_DEBUG
	, m_UsedBlockCount(0ull)
	, m_FreeBlockCount(1ull)
	, TotalFreeBytes(0ull)

	// 2단계
	, AccumulatedRequestSize(0ull)
	, AccumulatedFreeSize(0ull)
	, TotalAllocationCount(0ull)
	, TotalDeallocationCount(0ull)
	, TotalReallocationCount(0ull)
	, TotalRequestCount(0ull)

	// 3단계
	, PeakUsedBlockCount(0ull)
	, PeakUsedBytes(0ull)
	, PeakAllocationCount(0ull)
	, PeakDeallocationCount(0ull)
	, PeakRequestCount(0ull)
	, PeakFragmentation(0ull) // (1 - (LargestFreeBlockSize / TotalFreeBytes)) * 100
	, PeakExternalFragmentation(0ull) // (1 - (LargestFreeBlockSize / TotalHeapSize)) * 100
	, PeakInternalFragmentation(0ull) // (AccumulatedFreeSize / AccumulatedRequestSize) * 100
	
	// 4단계
	, PeakTotalFragmentation(0ull) // (1 - (LargestFreeBlockSize / TotalHeapSize)) * 100
	, PeakFragmentationAtAllocationFailure(0ull) // Fragmentation when an allocation fails due to fragmentation
	, PeakExternalFragmentationAtAllocationFailure(0ull) // External fragmentation when an allocation fails due to fragmentation
	, PeakInternalFragmentationAtAllocationFailure(0ull) // Internal fragmentation when an allocation fails due to fragmentation
	, PeakTotalFragmentationAtAllocationFailure(0ull) // Total fragmentation when an allocation fails due to fragmentation

	, TotalFailedAllocationCount(0ull)
	#endif
{
	// Allocate a big chunk of memory for the heap
	HeapStart = malloc(TotalHeapSize + sizeof(BlockHeader));

	if (nullptr == HeapStart)
	{
		LOG_ERROR(L"HeapAllocator: Failed to allocate heap memory", 1);
		abort();
		return;
	}

	// Initialize the RB-tree with a single large free block
	BlockHeader* InitialBlock = reinterpret_cast<BlockHeader*>(HeapStart);

	InitialBlock->BlockSize = TotalHeapSize + sizeof(BlockHeader);

	InitialBlock->IsFree        = true;
	InitialBlock->NextPhysBlock = nullptr;
	InitialBlock->PrevPhysBlock = nullptr;
#if RB_TREE_DEBUG
	InitialBlock->CanaryValue   = ECanary::Free;

	m_UsedBlockCount = 0;
	m_FreeBlockCount = 1;
	TotalFreeBytes = InitialBlock->BlockSize;
#endif

	FreeBlocksTree.Insert(InitialBlock);
}

Core::HeapAllocator::~HeapAllocator()
{
	if (nullptr != HeapStart)
	{
#if RB_TREE_DEBUG
		BlockHeader* CurrentBlock = reinterpret_cast<BlockHeader*>(HeapStart);

        size_t UsedBlockCount = 0;

		      size_t GuardCount    = 0;
        const size_t MaxGuardCount = 100000; // To prevent infinite loop in case of corrupted blocks

		while (nullptr != CurrentBlock)
		{
			CHECKF(GuardCount < MaxGuardCount, L"HeapAllocator::~HeapAllocatordetected possible corrupted physical block chain", 1);
			++GuardCount;

			if(false == CurrentBlock->IsFree)
			{
				++UsedBlockCount;
			}

            CurrentBlock = CurrentBlock->NextPhysBlock;
		}

        CHECKF(0 == UsedBlockCount, L"HeapAllocator::~HeapAllocator detected memory leak: there are still used blocks in the heap", 1);
#endif

		free(HeapStart);
		HeapStart = nullptr;
	}

	TotalHeapSize = 0;
}

void* Core::HeapAllocator::Allocate(size_t InputRequestSize, size_t InputAlignment)
{
#if RB_TREE_DEBUG
	++TotalRequestCount;
#endif

	if (0 == InputRequestSize)
	{
		return nullptr;
	}

	// size_t Alignment          = (MAP::sMinAlignment < InputAlignment) ? MAP::sDefaultAlignment : MAP::sMinAlignment;
	// size_t AlignedRequestSize = (InputRequestSize + Alignment - 1) & ~(Alignment - 1);

    const size_t AlignedRequestSize = MAP::AlignUp(InputRequestSize, InputAlignment);

#if RB_TREE_DEBUG
	AccumulatedRequestSize += InputRequestSize;
#endif

	// ArrSize of [Header] + [Usable Memory] 
	size_t ActualAllocationSize = AlignedRequestSize + sizeof(BlockHeader);

	BlockHeader* BestFitBlock = FreeBlocksTree.FindBestFit(ActualAllocationSize);

	if (nullptr == BestFitBlock)
	{
#if RB_TREE_DEBUG
		++TotalFailedAllocationCount;
#endif
		return nullptr;
	}

#if RB_TREE_DEBUG
	CHECKF(BestFitBlock->IsFree, L"Found a non-free block while searching for best fit", 1);
	CHECKF(ECanary::Free == BestFitBlock->CanaryValue, L"Found a corrupted block while searching for best fit", 1);
	
	--m_FreeBlockCount;
	TotalFreeBytes -= BestFitBlock->BlockSize;
#endif

	FreeBlocksTree.Remove(BestFitBlock);

	// 쓰고 남은 공간이 또 다른 블록이 될 수 있을 만큼 
	// 충분히 큰 경우에만 블록을 분할
	size_t RemainingSize = BestFitBlock->BlockSize - ActualAllocationSize;

	// Split the block if the remaining size is large enough to hold another block 
	// (including its header)
	// 
	// If ReaminingSize is equal to sizeof(BlockHeader), we can't split the block, 
	// because the new block would have no usable memory, 
	// so we require RemainingSize to be greater than sizeof(BlockHeader)
	if (sizeof(BlockHeader) < RemainingSize)
	{
		BlockHeader* SplitRemainderBlock = reinterpret_cast<BlockHeader*>
			(reinterpret_cast<char*>(BestFitBlock) + ActualAllocationSize);

		SplitRemainderBlock->BlockSize = RemainingSize;
		SplitRemainderBlock->IsFree    = true;
		SplitRemainderBlock->IsRed     = false;

		SplitRemainderBlock->Left      = nullptr;
		SplitRemainderBlock->Right     = nullptr;
		SplitRemainderBlock->Parent    = nullptr;

		// Update physical links
		SplitRemainderBlock->NextPhysBlock = BestFitBlock->NextPhysBlock;
		SplitRemainderBlock->PrevPhysBlock = BestFitBlock;
		
#if RB_TREE_DEBUG
		SplitRemainderBlock->CanaryValue = ECanary::Free;
#endif

		if (BestFitBlock->NextPhysBlock)
		{
			BestFitBlock->NextPhysBlock->PrevPhysBlock = SplitRemainderBlock;
		}

		BestFitBlock->NextPhysBlock = SplitRemainderBlock;
		BestFitBlock->BlockSize     = ActualAllocationSize;

		FreeBlocksTree.Insert(SplitRemainderBlock);

#if RB_TREE_DEBUG
		++m_FreeBlockCount;
		TotalFreeBytes += SplitRemainderBlock->BlockSize;
#endif
	}

	BestFitBlock->IsFree = false;

#if RB_TREE_DEBUG
	BestFitBlock->CanaryValue = ECanary::Used; // Set canary value for debugging

	++TotalAllocationCount;
	++m_UsedBlockCount;
#endif
	
	// Return a pointer to the usable memory (after the header)
	return reinterpret_cast<void*>(reinterpret_cast<char*>(BestFitBlock) + sizeof(BlockHeader));
}


// 막간 용어 정리
// BlockSize = sizeof(BlockHeader) + Payload
void* Core::HeapAllocator::Reallocate
(
	void*  InputPtr, 
	size_t InputNewSize, 
	size_t InputNewAlignment
)
{
#if RB_TREE_DEBUG
	++TotalReallocationCount;
#endif // RB_TREE_DEBUG


	// Realloc(nullptr, Size)는 Allocate(Size)랑 같음
	if (nullptr == InputPtr)
	{
		return Allocate(InputNewSize, InputNewAlignment);
	}

	// Reallocate(Ptr, 0)은 Deallocate(Ptr)이랑 같음
	if (0 == InputNewSize)
	{
		Deallocate(InputPtr);
		return nullptr;
	}

	const size_t Alignment             = MemoryAlignmentPolicy::AdjustAlignment(InputNewAlignment);
	const size_t AlignedNewPayloadSize = MemoryAlignmentPolicy::AlignUp(InputNewSize, Alignment);

	const size_t RequiredBlockSize = AlignedNewPayloadSize + sizeof(BlockHeader);

	BlockHeader* OldBlock = reinterpret_cast<BlockHeader*>
		(
			reinterpret_cast<char*>(InputPtr) - sizeof(BlockHeader)
		);

#if RB_TREE_DEBUG
	CHECKF(false == OldBlock->IsFree,
		L"HeapAllocator::Reallocate called with a pointer that is already free.", 1);

	CHECKF(ECanary::Used == OldBlock->CanaryValue,
		L"HeapAllocator::Reallocate called with a corrupted used block.", 1);
#endif

	const size_t OldBlockSize    = OldBlock->BlockSize;
	const size_t CopyPayloadSize = (OldBlockSize < InputNewSize)? OldBlockSize : InputNewSize;

	// -----------------------------------------------------
	// 1. Shrink or same size : 현재 블록 안에서 해결 가능
	// -----------------------------------------------------
	if (RequiredBlockSize <= OldBlockSize)
	{
		SplitUsedBlock(OldBlock, RequiredBlockSize);

		return InputPtr; // 포인터는 유지
	}
	
	// ------------------------------------------------------------
	// 2. Expand in-place : 바로 뒤 물리 블록이 free면 합쳐서 확장
	// ------------------------------------------------------------
	BlockHeader* NextBlock = OldBlock->NextPhysBlock;

	if (nullptr != NextBlock
		&& true == NextBlock->IsFree)
	{
		const size_t CombinedBlockSize =
			   OldBlock->BlockSize
			+ NextBlock->BlockSize;

		if (RequiredBlockSize <= CombinedBlockSize)
		{
#if RB_TREE_DEBUG
			--m_FreeBlockCount;
#endif

			FreeBlocksTree.Remove(NextBlock);

			// OldBlock에 NextBlock coalesce
			OldBlock->BlockSize     = CombinedBlockSize;
			OldBlock->NextPhysBlock = NextBlock->NextPhysBlock;
			OldBlock->IsFree        = false;

			if (nullptr != NextBlock->NextPhysBlock)
			{
				NextBlock->NextPhysBlock->PrevPhysBlock = OldBlock;
			}


#if RB_TREE_DEBUG
			OldBlock->CanaryValue = ECanary::Used;
#endif

			// 필요한 크기보다 커졌으면 뒤쪽을 다시 Split
			SplitUsedBlock(OldBlock, RequiredBlockSize);

			return InputPtr;
		} // end if (AlignedNewPayloadSize <= CombinedBlockSize)
		//  2. Expand in-place: 바로 뒤 물리 블록이 free면 합쳐서 확장 시도

	}
	
	// ----------------------------------------------
	// 3. 제자리 처리 불가능 : 새 블록 할당 후 복사
	// ----------------------------------------------
		
	void* NewPtr = Allocate(InputNewSize, InputNewAlignment);

	if (nullptr == NewPtr)
	{
		return nullptr;
	}

	const size_t CopySize = (OldBlockSize < InputNewSize)
		? OldBlockSize
		: InputNewSize;

	std::memcpy(NewPtr, InputPtr, CopySize);

	Deallocate(InputPtr);

	return NewPtr;
}

void Core::HeapAllocator::Deallocate(void* InputPtr)
{
#if RB_TREE_DEBUG
	++TotalRequestCount;
#endif

	if (nullptr == InputPtr)
	{
		return;
	}

	BlockHeader* BlockToFree = reinterpret_cast<BlockHeader*>(reinterpret_cast<char*>(InputPtr) - sizeof(BlockHeader));
	
#if RB_TREE_DEBUG

	CHECKF(!BlockToFree->IsFree, L"Attempting to free a block that is already free", 1); // Double free check
	CHECKF(ECanary::Used == BlockToFree->CanaryValue, L"Attempting to free a block with a corrupted canary value", 1); // Canary value check
	
	++TotalDeallocationCount;
	AccumulatedFreeSize += BlockToFree->BlockSize;
	TotalFreeBytes      += BlockToFree->BlockSize;
#endif
	
	// Coalesce with previous block if it's free
	if (nullptr != BlockToFree->PrevPhysBlock && BlockToFree->PrevPhysBlock->IsFree)
	{
		BlockHeader* PrevBlock = BlockToFree->PrevPhysBlock;

#if RB_TREE_DEBUG
		CHECKF(PrevBlock->IsFree, L"Previous block is not free", 1); // Prev block should be free
		--m_FreeBlockCount;
#endif
		FreeBlocksTree.Remove(PrevBlock);

		// Coalesce the blocks
		PrevBlock->BlockSize     += BlockToFree->BlockSize; // BlockSize 크기 == Payload 크기
		PrevBlock->NextPhysBlock  = BlockToFree->NextPhysBlock;

		if (BlockToFree->NextPhysBlock)
		{
			BlockToFree->NextPhysBlock->PrevPhysBlock = PrevBlock;
		}

		BlockToFree = PrevBlock; // Update BlockToFree to the coalesced block
	}

	// Coalesce with next block if it's free
	if (nullptr != BlockToFree->NextPhysBlock && BlockToFree->NextPhysBlock->IsFree)
	{
		BlockHeader* NextBlock = BlockToFree->NextPhysBlock;

#if RB_TREE_DEBUG
		CHECKF(NextBlock->IsFree, L"Next block is not free", 1); // Next block should be free
		--m_FreeBlockCount;
#endif
		FreeBlocksTree.Remove(NextBlock);

		// Coalesce the blocks
		BlockToFree->BlockSize     += NextBlock->BlockSize; // BlockSize 크기 == Payload 크기
		BlockToFree->NextPhysBlock  = NextBlock->NextPhysBlock;

		if (NextBlock->NextPhysBlock)
		{
			NextBlock->NextPhysBlock->PrevPhysBlock = BlockToFree;
		}
	}

	BlockToFree->IsFree = true;
#if RB_TREE_DEBUG
	BlockToFree->CanaryValue = ECanary::Free; // Set canary value for debugging
	++m_FreeBlockCount;
	--m_UsedBlockCount;
#endif

	FreeBlocksTree.Insert(BlockToFree);
}

#if RB_TREE_DEBUG

bool Core::HeapAllocator::ValidateHeap() const
{
	if (nullptr == HeapStart)
	{
		LOG_ERROR(L"HeapAllocator::ValidateHeap - Heap is empty.", 1);
		return false;
	}

	const char* HeapBegin = reinterpret_cast<const char*>(HeapStart);
	const char* HeapEnd   = HeapBegin + TotalHeapSize + sizeof(BlockHeader);

	const BlockHeader* CurrentBlock = reinterpret_cast<BlockHeader*>(HeapStart);
	const BlockHeader* PrevBlock = nullptr;

	while (nullptr != CurrentBlock)
	{
		const char* CurrentAddr = reinterpret_cast<const char*>(CurrentBlock);

		// 블록이 힙 범위 안에 있는지
		if (CurrentAddr < HeapBegin || HeapEnd <= CurrentAddr)
		{
			LOG_ERROR(L"HeapAllocator::ValidateHeap - Block is out of heap bounds.", 1);
			return false;
		}

		// 물리 링크가 맞는지
		if (CurrentBlock->PrevPhysBlock != PrevBlock)
		{
			LOG_ERROR(L"HeapAllocator::ValidateHeap - Physical block links are corrupted.", 1);
			return false;
		}

		// Free상태와 Canary가 맞는지
		if (CurrentBlock->IsFree)
		{
			if (ECanary::Free != CurrentBlock->CanaryValue)
			{
				LOG_ERROR(L"HeapAllocator::ValidateHeap - Free block has incorrect canary value.", 1);
				return false;
			}
		}
		else
		{
			if (ECanary::Used != CurrentBlock->CanaryValue)
			{
				LOG_ERROR(L"HeapAllocator::ValidateHeap - Used block has incorrect canary value.", 1);
				return false;
			}
		}

		if (CurrentBlock->BlockSize < sizeof(BlockHeader))
		{
			LOG_ERROR(L"HeapAllocator::ValidateHeap - Block size is too small to hold a header.", 1);
			return false;
		}

		const char* NextBlockAddr = CurrentAddr + CurrentBlock->BlockSize;

		if (HeapEnd < NextBlockAddr)
		{
			LOG_ERROR(L"HeapAllocator::ValidateHeap - Block size extends beyond heap bounds.", 1);
			return false;
		}

		PrevBlock    = CurrentBlock;

		// 테스트 종료
		if (HeapEnd == NextBlockAddr)
		{
			break;
		}

		CurrentBlock = reinterpret_cast<const BlockHeader*>(NextBlockAddr);
	}

	return true;
}

void Core::HeapAllocator::PrintMemoryLayout() const
{
	BlockHeader* CurrentBlock = reinterpret_cast<BlockHeader*>(HeapStart);

	while (CurrentBlock)
	{
		printf("[%s | Size: %zu] ",
			CurrentBlock->IsFree ? "Free" : "Used",
			CurrentBlock->BlockSize);

		CurrentBlock = CurrentBlock->NextPhysBlock;
	}
}

void Core::HeapAllocator::PrintFragmentation() const
{
	BlockHeader* CurrentBlock = reinterpret_cast<BlockHeader*>(HeapStart);

	if (nullptr == CurrentBlock)
	{
		printf("HeapAllocator::PrintFramentation : Heap is empty.\n");
		return;
	}

	size_t TotalFreeSize         = 0;

	size_t LargestFreeBlockSize  = 0;
	size_t SmallestFreeBlockSize = SIZE_MAX;

	BlockHeader* LargestFreeBlock  = nullptr;
	BlockHeader* SmallestFreeBlock = nullptr;

	size_t FreeBlockCount        = 0;
	size_t UsedBlockCount        = 0;

	while (nullptr != CurrentBlock)
	{
		if (CurrentBlock->IsFree)
		{
			TotalFreeSize += CurrentBlock->BlockSize;
			++FreeBlockCount;

			if (LargestFreeBlockSize < CurrentBlock->BlockSize)
			{
				LargestFreeBlockSize = CurrentBlock->BlockSize;
				LargestFreeBlock = CurrentBlock;
			}

			if (CurrentBlock->BlockSize < SmallestFreeBlockSize)
			{
				SmallestFreeBlockSize = CurrentBlock->BlockSize;
				SmallestFreeBlock = CurrentBlock;
			}
		}
		else
		{
			++UsedBlockCount;
		}

		CurrentBlock = CurrentBlock->NextPhysBlock;
	}

	printf("Total Free Size: %zu bytes\n", TotalFreeSize);

	printf("Largest  Free Block: [%p] %zu bytes\n", LargestFreeBlock, LargestFreeBlockSize);
	
	if (nullptr != SmallestFreeBlock)
	{
		printf("Smallest Free Block: [%p] %zu bytes\n", SmallestFreeBlock, SmallestFreeBlockSize);
	}
	else
	{
		printf("Smallest Free Block : None\n");
	}
	
	printf("Free Block Count: %zu\n", FreeBlockCount);
	printf("Used Block Count: %zu\n", UsedBlockCount);

	// 0.0 = No fragmentation (all free memory is in one block)
	// 0.5 = Moderate fragmentation (free memory is split into multiple blocks, but not too bad)
	// 0.99 = Severe fragmentation (free memory is split into many small blocks, making it hard to find large contiguous free blocks)
	if (0 < TotalFreeSize)
	{
		double Fragmentation = 1.0 - (static_cast<double>(LargestFreeBlockSize) / TotalFreeSize);
		printf("Fragmentation: %.2f%%\n", Fragmentation * 100.0);
	}

	CHECKF(m_UsedBlockCount == UsedBlockCount, L"m_UsedBlockCount mismatch", 1);
	CHECKF(m_FreeBlockCount == FreeBlockCount, L"m_FreeBlockCount mismatch", 1);
	CHECKF(TotalFreeBytes   == TotalFreeSize,  L"TotalFreeBytes mismatch",   1);
}

#endif // #if RB_TREE_DEBUG

// Private Func
Core::BlockHeader* Core::HeapAllocator::SplitUsedBlock
(
	BlockHeader* InputBlock, 
	size_t       InputUsedBlockSize // InputBlock의 InputUsedPayloadSize만 남기고 뒤쪽을 free block
)
{
	/*
	const size_t InputBlockOldSize = InputBlock->BlockSize;

	if (InputBlockOldSize <= InputUsedPayloadSize)
	{
		return nullptr;
	}

	const size_t RemaningTotalSize = InputBlockOldSize - InputUsedPayloadSize;

	if (RemaningTotalSize < sizeof(BlockHeader) + MemoryAlignmentPolicy::sMinAlignment)
	{
		return nullptr;
	}
	*/

	if (nullptr == InputBlock)
	{
		return nullptr;
	}

	if (InputBlock->BlockSize <= InputUsedBlockSize)
	{
		return nullptr;
	}

	const size_t RemainingBlockSize = InputBlock->BlockSize - InputUsedBlockSize;
	const size_t SmallestBlockSize  = sizeof(BlockHeader) + MemoryAlignmentPolicy::sMinAlignment;

	if (RemainingBlockSize < SmallestBlockSize)
	{
		return nullptr;
	}

	char* NewBlockPtr =
		reinterpret_cast<char*>(InputBlock)
		+ InputUsedBlockSize;

	BlockHeader* NewFreeBlock = reinterpret_cast<BlockHeader*>(NewBlockPtr);

	NewFreeBlock->BlockSize = RemainingBlockSize;
	NewFreeBlock->IsFree    = true;
	NewFreeBlock->PrevPhysBlock = InputBlock;
	NewFreeBlock->NextPhysBlock = InputBlock->NextPhysBlock;

	if (nullptr != InputBlock->NextPhysBlock)
	{
		InputBlock->NextPhysBlock->PrevPhysBlock = NewFreeBlock;
	}
	
	InputBlock->BlockSize     = InputUsedBlockSize;
	InputBlock->NextPhysBlock = NewFreeBlock;

#if RB_TREE_DEBUG
	NewFreeBlock->CanaryValue = ECanary::Free;
	++m_FreeBlockCount;
#endif

	FreeBlocksTree.Insert(NewFreeBlock);

	return NewFreeBlock;
}


#pragma endregion HeapAllocator Implementation

