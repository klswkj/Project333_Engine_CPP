#include "gtest/gtest.h"
#include "../Source/Core/Public/Memory/Allocator/HeapAllocator.h"
#include "../Source/Core/Public/Memory/Container/Array.h"
#include "../Source/Core/Public/Memory/Memory.h"

#include <string>

class ArrayTest : public ::testing::Test
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

struct FArrayTrackedObject
{
	static int ConstructCount;
	static int DestructCount;
	static int CopyAssignCount;

	int Value;

	FArrayTrackedObject()
		: Value(0)
	{
		++ConstructCount;
	}

	explicit FArrayTrackedObject(int InputValue)
		: Value(InputValue)
	{
		++ConstructCount;
	}

	FArrayTrackedObject& operator=(const FArrayTrackedObject& InputOther)
	{
		if (this != &InputOther)
		{
			Value = InputOther.Value;
			++CopyAssignCount;
		}

		return *this;
	}

	~FArrayTrackedObject()
	{
		++DestructCount;
	}

	static void Reset()
	{
		ConstructCount = 0;
		DestructCount = 0;
		CopyAssignCount = 0;
	}
};

int FArrayTrackedObject::ConstructCount = 0;
int FArrayTrackedObject::DestructCount = 0;
int FArrayTrackedObject::CopyAssignCount = 0;

TEST_F(ArrayTest, UsesInlineAllocatorWithinInlineCapacity)
{
	using AllocatorType = Core::InlineAllocator<int, 8>;

	Core::Array<int, AllocatorType> Values(4);

	Values[0] = 10;
	Values[1] = 20;
	Values[2] = 30;
	Values[3] = 40;

	EXPECT_EQ(4, Values.Size());
	EXPECT_EQ(10, Values[0]);
	EXPECT_EQ(20, Values[1]);
	EXPECT_EQ(30, Values[2]);
	EXPECT_EQ(40, Values[3]);

	EXPECT_TRUE(Values.GetAllocator().IsInlineStorage(Values.Data()));
}

TEST_F(ArrayTest, InlineAllocatorUsesSecondaryAllocatorWhenSizeExceedsInlineCapacity)
{
	using AllocatorType = Core::InlineAllocator<int, 4>;

	Core::Array<int, AllocatorType> Values(8);

	Values[0] = 10;
	Values[7] = 80;

	EXPECT_EQ(8,  Values.Size());
	EXPECT_EQ(10, Values[0]);
	EXPECT_EQ(80, Values[7]);

	EXPECT_FALSE(Values.GetAllocator().IsInlineStorage(Values.Data()));
}

TEST_F(ArrayTest, InlineAllocatorWorksWithStringElements)
{
	using AllocatorType = Core::InlineAllocator<std::string, 4>;

	Core::Array<std::string, AllocatorType> Values(3);

	Values[0] = "AA";
	Values[1] = "BB";
	Values[2] = "CC";

	EXPECT_STREQ("AA", Values[0].c_str());
	EXPECT_STREQ("BB", Values[1].c_str());
	EXPECT_STREQ("CC", Values[2].c_str());

	EXPECT_TRUE(Values.GetAllocator().IsInlineStorage(Values.Data()));
}

TEST_F(ArrayTest, InlineAllocatorStringElementsUseSecondaryAllocatorWhenSizeExceedsInlineCapacity)
{
	using AllocatorType = Core::InlineAllocator<std::string, 2>;

	Core::Array<std::string, AllocatorType> Values(3);

	Values[0] = "AA";
	Values[1] = "BB";
	Values[2] = "CC";

	EXPECT_STREQ("AA", Values[0].c_str());
	EXPECT_STREQ("BB", Values[1].c_str());
	EXPECT_STREQ("CC", Values[2].c_str());

	EXPECT_FALSE(Values.GetAllocator().IsInlineStorage(Values.Data()));
}