#include <gtest/gtest.h>

#include <type_traits>

#include "../Source/Core/Public/Memory/Container/Queue.h"
#include "../Source/Core/Public/Memory/Memory.h"

static_assert
(
	std::is_same_v
	<
		Core::FixedQueue<int, 3>,
		Core::Queue<int, 3, Core::InlineAllocator<int, 3>, true>
	>,
	"Core::FixedQueue must use Core::InlineAllocator."
);

class QueueTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		Core::Memory::Initialize();
	}

	void TearDown() override
	{
		Core::Memory::Shutdown();
	}
};

struct FQueueMoveOnly
{
	int Value = 0;

	FQueueMoveOnly() = delete;

	explicit FQueueMoveOnly(int InputValue)
		: Value(InputValue)
	{
	}

	FQueueMoveOnly(const FQueueMoveOnly& ) = delete;
	FQueueMoveOnly(      FQueueMoveOnly&& InputOther) noexcept
		: Value(InputOther.Value)
	{
		InputOther.Value = -1;
	}

	FQueueMoveOnly& operator=(const FQueueMoveOnly& ) = delete;
	FQueueMoveOnly& operator=(      FQueueMoveOnly&& InputOther) noexcept
	{
		if (this != &InputOther)
		{
			Value = InputOther.Value;
			InputOther.Value = -1;
		}

		return *this;
	}
};

struct FQueueTrackedObject
{
	static int ConstructCount;
	static int DestructCount;

	int Value = 0;

	explicit FQueueTrackedObject(int InputValue)
		: Value(InputValue)
	{
		++ConstructCount;
	}

	FQueueTrackedObject(const FQueueTrackedObject& InputOther)
		: Value(InputOther.Value)
	{
		++ConstructCount;
	}

	FQueueTrackedObject(FQueueTrackedObject&& InputOther) noexcept
		: Value(InputOther.Value)
	{
		++ConstructCount;
		InputOther.Value = -1;
	}

	FQueueTrackedObject& operator=(const FQueueTrackedObject& ) = delete;
	FQueueTrackedObject& operator=(      FQueueTrackedObject&& InputOther) noexcept
	{
		if (this != &InputOther)
		{
			Value = InputOther.Value;
			InputOther.Value = -1;
		}

		return *this;
	}

	~FQueueTrackedObject()
	{
		++DestructCount;
	}

	static void Reset()
	{
		ConstructCount = 0;
		DestructCount  = 0;
	}
};

int FQueueTrackedObject::ConstructCount = 0;
int FQueueTrackedObject::DestructCount  = 0;

TEST_F(QueueTest, DynamicQueueDequeuesInFifoOrder)
{
	Core::DynamicQueue<int> Queue;

	EXPECT_TRUE(Queue.IsEmpty());
	EXPECT_EQ(0, Queue.GetSize());

	EXPECT_TRUE(Queue.Enqueue(10));
	EXPECT_TRUE(Queue.Enqueue(20));
	EXPECT_TRUE(Queue.Enqueue(30));

	EXPECT_FALSE(Queue.IsEmpty());
	EXPECT_EQ(3, Queue.GetSize());
	ASSERT_NE(nullptr, Queue.Peek());
	EXPECT_EQ(10, *Queue.Peek());

	int Value = 0;
	EXPECT_TRUE(Queue.Dequeue(Value));
	EXPECT_EQ(10, Value);
	EXPECT_TRUE(Queue.Dequeue(Value));
	EXPECT_EQ(20, Value);
	EXPECT_TRUE(Queue.Dequeue(Value));
	EXPECT_EQ(30, Value);

	EXPECT_FALSE(Queue.Dequeue(Value));
	EXPECT_TRUE(Queue.IsEmpty());
	EXPECT_EQ(0, Queue.GetSize());
	EXPECT_EQ(nullptr, Queue.Peek());
}

TEST_F(QueueTest, DynamicQueueClearsConsumedStorage)
{
	Core::DynamicQueue<int> Queue;

	EXPECT_TRUE(Queue.Enqueue(10));
	EXPECT_TRUE(Queue.Enqueue(20));

	Queue.Pop();

	EXPECT_EQ(1, Queue.GetSize());
	ASSERT_NE(nullptr, Queue.Peek());
	EXPECT_EQ(20, *Queue.Peek());

	Queue.Pop();

	EXPECT_TRUE(Queue.IsEmpty());
	EXPECT_EQ(0, Queue.GetSize());

	EXPECT_TRUE(Queue.Enqueue(30));
	ASSERT_NE(nullptr, Queue.Peek());
	EXPECT_EQ(30, *Queue.Peek());
}

TEST_F(QueueTest, FixedQueueRejectsEnqueueWhenFull)
{
	Core::FixedQueue<int, 3> Queue;

	EXPECT_TRUE(Queue.IsEmpty());
	EXPECT_EQ(3, Queue.GetCapacity());

	EXPECT_TRUE(Queue.Enqueue(10));
	EXPECT_TRUE(Queue.Enqueue(20));
	EXPECT_TRUE(Queue.Enqueue(30));

	EXPECT_TRUE(Queue.IsFull());
	EXPECT_FALSE(Queue.Enqueue(40));
	EXPECT_EQ(3, Queue.GetSize());
}

TEST_F(QueueTest, FixedQueueWrapsAroundInFifoOrder)
{
	Core::FixedQueue<int, 3> Queue;

	EXPECT_TRUE(Queue.Enqueue(10));
	EXPECT_TRUE(Queue.Enqueue(20));
	EXPECT_TRUE(Queue.Enqueue(30));

	int Value = 0;
	EXPECT_TRUE(Queue.Dequeue(Value));
	EXPECT_EQ(10, Value);
	EXPECT_TRUE(Queue.Dequeue(Value));
	EXPECT_EQ(20, Value);

	EXPECT_TRUE(Queue.Enqueue(40));
	EXPECT_TRUE(Queue.Enqueue(50));
	EXPECT_TRUE(Queue.IsFull());

	EXPECT_TRUE(Queue.Dequeue(Value));
	EXPECT_EQ(30, Value);
	EXPECT_TRUE(Queue.Dequeue(Value));
	EXPECT_EQ(40, Value);
	EXPECT_TRUE(Queue.Dequeue(Value));
	EXPECT_EQ(50, Value);

	EXPECT_TRUE(Queue.IsEmpty());
	EXPECT_FALSE(Queue.Dequeue(Value));
}

TEST_F(QueueTest, FixedQueueCanEmplaceNonDefaultConstructibleValues)
{
	Core::FixedQueue<FQueueMoveOnly, 2> Queue;

	EXPECT_TRUE(Queue.Emplace(10));
	EXPECT_TRUE(Queue.Emplace(20));
	EXPECT_TRUE(Queue.IsFull());

	FQueueMoveOnly Value(0);
	EXPECT_TRUE(Queue.Dequeue(Value));
	EXPECT_EQ(10, Value.Value);
	EXPECT_TRUE(Queue.Dequeue(Value));
	EXPECT_EQ(20, Value.Value);
	EXPECT_TRUE(Queue.IsEmpty());
}

TEST_F(QueueTest, FixedQueueDestroysConstructedSlots)
{
	FQueueTrackedObject::Reset();

	{
		Core::FixedQueue<FQueueTrackedObject, 3> Queue;

		EXPECT_TRUE(Queue.Emplace(10));
		EXPECT_TRUE(Queue.Emplace(20));
		EXPECT_EQ(2, FQueueTrackedObject::ConstructCount);
		EXPECT_EQ(0, FQueueTrackedObject::DestructCount);

		Queue.Pop();

		EXPECT_EQ(2, FQueueTrackedObject::ConstructCount);
		EXPECT_EQ(1, FQueueTrackedObject::DestructCount);

		Queue.Clear();

		EXPECT_EQ(2, FQueueTrackedObject::ConstructCount);
		EXPECT_EQ(2, FQueueTrackedObject::DestructCount);
	}

	EXPECT_EQ(FQueueTrackedObject::ConstructCount, FQueueTrackedObject::DestructCount);
}

TEST_F(QueueTest, FixedQueuePopAllowsReuse)
{
	Core::FixedQueue<int, 3> Queue;

	EXPECT_TRUE(Queue.Enqueue(10));
	EXPECT_TRUE(Queue.Enqueue(20));
	EXPECT_TRUE(Queue.Enqueue(30));
	EXPECT_TRUE(Queue.IsFull());

	Queue.Pop();
	Queue.Pop();

	EXPECT_EQ(1, Queue.GetSize());
	ASSERT_NE(nullptr, Queue.Peek());
	EXPECT_EQ(30, *Queue.Peek());

	EXPECT_TRUE(Queue.Enqueue(40));
	EXPECT_TRUE(Queue.Enqueue(50));
	EXPECT_TRUE(Queue.IsFull());

	int Value = 0;
	EXPECT_TRUE(Queue.Dequeue(Value));
	EXPECT_EQ(30, Value);
	EXPECT_TRUE(Queue.Dequeue(Value));
	EXPECT_EQ(40, Value);
	EXPECT_TRUE(Queue.Dequeue(Value));
	EXPECT_EQ(50, Value);

	EXPECT_TRUE(Queue.IsEmpty());
}

TEST_F(QueueTest, FixedQueueCanEnqueueAfterClear)
{
	Core::FixedQueue<int, 3> Queue;

	EXPECT_TRUE(Queue.Enqueue(10));
	EXPECT_TRUE(Queue.Enqueue(20));
	EXPECT_TRUE(Queue.Enqueue(30));

	Queue.Clear();

	EXPECT_TRUE(Queue.IsEmpty());
	EXPECT_EQ(0, Queue.GetSize());
	EXPECT_FALSE(Queue.IsFull());

	EXPECT_TRUE(Queue.Enqueue(40));
	EXPECT_TRUE(Queue.Enqueue(50));

	int Value = 0;
	EXPECT_TRUE(Queue.Dequeue(Value));
	EXPECT_EQ(40, Value);
	EXPECT_TRUE(Queue.Dequeue(Value));
	EXPECT_EQ(50, Value);
}

TEST_F(QueueTest, FixedQueueWorksWithCapacityOne)
{
	Core::FixedQueue<int, 1> Queue;

	EXPECT_TRUE(Queue.IsEmpty());
	EXPECT_FALSE(Queue.IsFull());

	EXPECT_TRUE(Queue.Enqueue(10));
	EXPECT_TRUE(Queue.IsFull());
	EXPECT_FALSE(Queue.Enqueue(20));

	int Value = 0;
	EXPECT_TRUE(Queue.Dequeue(Value));
	EXPECT_EQ(10, Value);

	EXPECT_TRUE(Queue.IsEmpty());

	EXPECT_TRUE(Queue.Enqueue(30));
	EXPECT_TRUE(Queue.Dequeue(Value));
	EXPECT_EQ(30, Value);
}

TEST_F(QueueTest, FixedQueuePeekAllowsMutation)
{
	Core::FixedQueue<int, 2> Queue;

	EXPECT_TRUE(Queue.Enqueue(10));

	int* ValuePtr = Queue.Peek();
	ASSERT_NE(nullptr, ValuePtr);

	*ValuePtr = 99;

	int Value = 0;
	EXPECT_TRUE(Queue.Dequeue(Value));
	EXPECT_EQ(99, Value);
}

TEST_F(QueueTest, FixedQueueDequeueDestroysConstructedSlot)
{
	FQueueTrackedObject::Reset();

	{
		Core::FixedQueue<FQueueTrackedObject, 2> Queue;

		EXPECT_TRUE(Queue.Emplace(10));
		EXPECT_EQ(1, FQueueTrackedObject::ConstructCount);
		EXPECT_EQ(0, FQueueTrackedObject::DestructCount);

		FQueueTrackedObject Value(0);

		EXPECT_TRUE(Queue.Dequeue(Value));

		EXPECT_EQ(2, FQueueTrackedObject::ConstructCount);
		EXPECT_EQ(1, FQueueTrackedObject::DestructCount);
	}

	EXPECT_EQ(FQueueTrackedObject::ConstructCount, FQueueTrackedObject::DestructCount);
}

TEST_F(QueueTest, FixedQueueFailedEnqueueDoesNotModifyQueue)
{
	Core::FixedQueue<int, 2> Queue;

	EXPECT_TRUE(Queue.Enqueue(10));
	EXPECT_TRUE(Queue.Enqueue(20));

	EXPECT_FALSE(Queue.Enqueue(30));
	EXPECT_EQ(2, Queue.GetSize());

	int Value = 0;
	EXPECT_TRUE(Queue.Dequeue(Value));
	EXPECT_EQ(10, Value);
	EXPECT_TRUE(Queue.Dequeue(Value));
	EXPECT_EQ(20, Value);
	EXPECT_TRUE(Queue.IsEmpty());
}
