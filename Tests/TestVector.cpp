#include <gtest/gtest.h>

#include "../Source/Core/Public/Memory/Allocator/HeapAllocator.h"
#include "../Source/Core/Public/Memory/Container/Vector.h"
#include "../Source/Core/Public/Memory/Memory.h"


class VectorTest : public ::testing::Test
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

struct FTrackedObject
{
	static int ConstructCount;
	static int DestructCount;
	static int CopyConstructCount;
	static int MoveConstructCount;
	static int MoveAssignCount;

	int Value;

	FTrackedObject()
		: Value(0)
	{
		++ConstructCount;
	}

	explicit FTrackedObject(int InputValue)
		: Value(InputValue)
	{
		++ConstructCount;
	}

	FTrackedObject(const FTrackedObject& InputOther)
		: Value(InputOther.Value)
	{
		++ConstructCount;
		++CopyConstructCount;
	}

	FTrackedObject(FTrackedObject&& InputOther) noexcept
		: Value(InputOther.Value)
	{
		++ConstructCount;
		++MoveConstructCount;
		InputOther.Value = -1;
	}

	FTrackedObject& operator=(FTrackedObject&& InputOther) noexcept
	{
		if (this != &InputOther)
		{
			Value = InputOther.Value;
			InputOther.Value = -1;
			++MoveAssignCount;
		}

		return *this;
	}

	~FTrackedObject()
	{
		++DestructCount;
	}

	static void Reset()
	{
		ConstructCount = 0;
		DestructCount = 0;
		CopyConstructCount = 0;
		MoveConstructCount = 0;
		MoveAssignCount = 0;
	}
};

int FTrackedObject::ConstructCount = 0;
int FTrackedObject::DestructCount = 0;
int FTrackedObject::CopyConstructCount = 0;
int FTrackedObject::MoveConstructCount = 0;
int FTrackedObject::MoveAssignCount = 0;

TEST_F(VectorTest, UsesInlineAllocatorWithinInlineCapacity)
{
	using AllocatorType = Core::InlineAllocator<int, 8>;

	Core::Vector<int, AllocatorType> Values;

	EXPECT_TRUE(Values.Push_back(10));
	EXPECT_TRUE(Values.Push_back(20));
	EXPECT_TRUE(Values.Push_back(30));

	EXPECT_EQ(3, Values.GetSize());
	EXPECT_EQ(10, Values[0]);
	EXPECT_EQ(20, Values[1]);
	EXPECT_EQ(30, Values[2]);

	EXPECT_TRUE(Values.GetAllocator().IsInlineStorage(Values.GetData()));
}

TEST_F(VectorTest, InlineAllocatorMovesToSecondaryAllocatorWhenCapacityExceeded)
{
	using AllocatorType = Core::InlineAllocator<int, 8>;

	Core::Vector<int, AllocatorType> Values;

	EXPECT_TRUE(Values.Push_back(10));
	EXPECT_TRUE(Values.Push_back(20));
	EXPECT_TRUE(Values.Push_back(30));
	EXPECT_TRUE(Values.Push_back(40));
	EXPECT_TRUE(Values.Push_back(50));
	EXPECT_TRUE(Values.Push_back(60));
	EXPECT_TRUE(Values.Push_back(70));
	EXPECT_TRUE(Values.Push_back(80));

	EXPECT_TRUE(Values.GetAllocator().IsInlineStorage(Values.GetData()));

	EXPECT_TRUE(Values.Push_back(90));

	EXPECT_EQ(9, Values.GetSize());
	EXPECT_EQ(10, Values[0]);
	EXPECT_EQ(80, Values[7]);
	EXPECT_EQ(90, Values[8]);

	EXPECT_FALSE(Values.GetAllocator().IsInlineStorage(Values.GetData()));
}

TEST_F(VectorTest, InlineAllocatorReserveBeyondInlineCapacityUsesSecondaryAllocator)
{
	using AllocatorType = Core::InlineAllocator<int, 8>;

	Core::Vector<int, AllocatorType> Values;

	EXPECT_TRUE(Values.Push_back(10));
	EXPECT_TRUE(Values.Push_back(20));

	EXPECT_TRUE(Values.GetAllocator().IsInlineStorage(Values.GetData()));

	EXPECT_TRUE(Values.Reserve(16));

	EXPECT_EQ(2, Values.GetSize());
	EXPECT_EQ(10, Values[0]);
	EXPECT_EQ(20, Values[1]);

	EXPECT_FALSE(Values.GetAllocator().IsInlineStorage(Values.GetData()));
}

TEST_F(VectorTest, InlineAllocatorWorksWithTrackedObjects)
{
	using AllocatorType = Core::InlineAllocator<FTrackedObject, 8>;

	FTrackedObject::Reset();

	{
		Core::Vector<FTrackedObject, AllocatorType> Values;

		EXPECT_TRUE(Values.Emplace_back(10));
		EXPECT_TRUE(Values.Emplace_back(20));
		EXPECT_TRUE(Values.Emplace_back(30));

		EXPECT_TRUE(Values.GetAllocator().IsInlineStorage(Values.GetData()));

		EXPECT_EQ(3, Values.GetSize());
		EXPECT_EQ(10, Values[0].Value);
		EXPECT_EQ(20, Values[1].Value);
		EXPECT_EQ(30, Values[2].Value);
	}

	EXPECT_EQ(FTrackedObject::ConstructCount, FTrackedObject::DestructCount);
}

TEST_F(VectorTest, InlineAllocatorTrackedObjectsMoveToSecondaryAllocator)
{
	using AllocatorType = Core::InlineAllocator<FTrackedObject, 8>;

	FTrackedObject::Reset();

	{
		Core::Vector<FTrackedObject, AllocatorType> Values;

		for (int i = 0; i < 8; ++i)
		{
			EXPECT_TRUE(Values.Emplace_back((i + 1) * 10));
		}

		EXPECT_TRUE(Values.GetAllocator().IsInlineStorage(Values.GetData()));

		EXPECT_TRUE(Values.Emplace_back(90));

		EXPECT_FALSE(Values.GetAllocator().IsInlineStorage(Values.GetData()));

		EXPECT_EQ(9, Values.GetSize());
		EXPECT_EQ(10, Values[0].Value);
		EXPECT_EQ(80, Values[7].Value);
		EXPECT_EQ(90, Values[8].Value);
	}

	EXPECT_EQ(FTrackedObject::ConstructCount, FTrackedObject::DestructCount);
}