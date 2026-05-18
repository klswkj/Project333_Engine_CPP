#include <gtest/gtest.h>

#include <type_traits>

#include "../Source/Core/Public/Memory/Container/Stack.h"
#include "../Source/Core/Public/Memory/Memory.h"

static_assert
(
    std::is_same_v
    <
        Core::FixedStack<int, 3>,
        Core::Stack<int, 3, Core::InlineAllocator<int, 3>, true>
    >,
    "Core::FixedStack must use Core::InlineAllocator."
);

class StackTest : public ::testing::Test
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

struct FStackMoveOnly
{
    int Value = 0;

    FStackMoveOnly() = delete;

    explicit FStackMoveOnly(int InputValue)
        : Value(InputValue)
    {
    }

    FStackMoveOnly(const FStackMoveOnly& ) = delete;
    FStackMoveOnly(      FStackMoveOnly&& InputOther) noexcept
        : Value(InputOther.Value)
    {
        InputOther.Value = -1;
    }

    FStackMoveOnly& operator=(const FStackMoveOnly& ) = delete;
    FStackMoveOnly& operator=(      FStackMoveOnly&& InputOther) noexcept
    {
        if (this != &InputOther)
        {
            Value = InputOther.Value;
            InputOther.Value = -1;
        }

        return *this;
    }
};

struct FStackTrackedObject
{
    static int ConstructCount;
    static int DestructCount;

    int Value = 0;

    explicit FStackTrackedObject(int InputValue)
        : Value(InputValue)
    {
        ++ConstructCount;
    }

    FStackTrackedObject(const FStackTrackedObject& InputOther)
        : Value(InputOther.Value)
    {
        ++ConstructCount;
    }

    FStackTrackedObject(FStackTrackedObject&& InputOther) noexcept
        : Value(InputOther.Value)
    {
        ++ConstructCount;
        InputOther.Value = -1;
    }

    FStackTrackedObject& operator=(const FStackTrackedObject& ) = delete;
    FStackTrackedObject& operator=(      FStackTrackedObject&& InputOther) noexcept
    {
        if (this != &InputOther)
        {
            Value = InputOther.Value;
            InputOther.Value = -1;
        }

        return *this;
    }

    ~FStackTrackedObject()
    {
        ++DestructCount;
    }

    static void Reset()
    {
        ConstructCount = 0;
        DestructCount  = 0;
    }
};

int FStackTrackedObject::ConstructCount = 0;
int FStackTrackedObject::DestructCount  = 0;

TEST_F(StackTest, DynamicStackPopsInLifoOrder)
{
    Core::DynamicStack<int> Stack;

    EXPECT_TRUE(Stack.IsEmpty());
    EXPECT_EQ(0, Stack.GetSize());

    EXPECT_TRUE(Stack.Push(10));
    EXPECT_TRUE(Stack.Push(20));
    EXPECT_TRUE(Stack.Push(30));

    EXPECT_FALSE(Stack.IsEmpty());
    EXPECT_EQ(3, Stack.GetSize());
    ASSERT_NE(nullptr, Stack.Peek());
    EXPECT_EQ(30, *Stack.Peek());

    int Value = 0;
    EXPECT_TRUE(Stack.Pop(Value));
    EXPECT_EQ(30, Value);
    EXPECT_TRUE(Stack.Pop(Value));
    EXPECT_EQ(20, Value);
    EXPECT_TRUE(Stack.Pop(Value));
    EXPECT_EQ(10, Value);

    EXPECT_FALSE(Stack.Pop(Value));
    EXPECT_TRUE(Stack.IsEmpty());
    EXPECT_EQ(nullptr, Stack.Peek());
}

TEST_F(StackTest, FixedStackRejectsPushWhenFull)
{
    Core::FixedStack<int, 3> Stack;

    EXPECT_TRUE(Stack.IsEmpty());
    EXPECT_EQ(3, Stack.GetCapacity());

    EXPECT_TRUE(Stack.Push(10));
    EXPECT_TRUE(Stack.Push(20));
    EXPECT_TRUE(Stack.Push(30));

    EXPECT_TRUE(Stack.IsFull());
    EXPECT_FALSE(Stack.Push(40));
    EXPECT_EQ(3, Stack.GetSize());
}

TEST_F(StackTest, FixedStackPopsInLifoOrder)
{
    Core::FixedStack<int, 3> Stack;

    EXPECT_TRUE(Stack.Push(10));
    EXPECT_TRUE(Stack.Push(20));
    EXPECT_TRUE(Stack.Push(30));

    int Value = 0;
    EXPECT_TRUE(Stack.Pop(Value));
    EXPECT_EQ(30, Value);
    EXPECT_TRUE(Stack.Pop(Value));
    EXPECT_EQ(20, Value);
    EXPECT_TRUE(Stack.Pop(Value));
    EXPECT_EQ(10, Value);

    EXPECT_TRUE(Stack.IsEmpty());
    EXPECT_FALSE(Stack.Pop(Value));
}

TEST_F(StackTest, FixedStackPopAllowsReuse)
{
    Core::FixedStack<int, 3> Stack;

    EXPECT_TRUE(Stack.Push(10));
    EXPECT_TRUE(Stack.Push(20));
    EXPECT_TRUE(Stack.Push(30));
    EXPECT_TRUE(Stack.IsFull());

    Stack.Pop();
    Stack.Pop();

    EXPECT_EQ(1, Stack.GetSize());
    ASSERT_NE(nullptr, Stack.Peek());
    EXPECT_EQ(10, *Stack.Peek());

    EXPECT_TRUE(Stack.Push(40));
    EXPECT_TRUE(Stack.Push(50));
    EXPECT_TRUE(Stack.IsFull());

    int Value = 0;
    EXPECT_TRUE(Stack.Pop(Value));
    EXPECT_EQ(50, Value);
    EXPECT_TRUE(Stack.Pop(Value));
    EXPECT_EQ(40, Value);
    EXPECT_TRUE(Stack.Pop(Value));
    EXPECT_EQ(10, Value);
}

TEST_F(StackTest, FixedStackCanPushAfterClear)
{
    Core::FixedStack<int, 3> Stack;

    EXPECT_TRUE(Stack.Push(10));
    EXPECT_TRUE(Stack.Push(20));
    EXPECT_TRUE(Stack.Push(30));

    Stack.Clear();

    EXPECT_TRUE(Stack.IsEmpty());
    EXPECT_EQ(0, Stack.GetSize());
    EXPECT_FALSE(Stack.IsFull());

    EXPECT_TRUE(Stack.Push(40));
    EXPECT_TRUE(Stack.Push(50));

    int Value = 0;
    EXPECT_TRUE(Stack.Pop(Value));
    EXPECT_EQ(50, Value);
    EXPECT_TRUE(Stack.Pop(Value));
    EXPECT_EQ(40, Value);
}

TEST_F(StackTest, FixedStackWorksWithCapacityOne)
{
    Core::FixedStack<int, 1> Stack;

    EXPECT_TRUE(Stack.IsEmpty());
    EXPECT_FALSE(Stack.IsFull());

    EXPECT_TRUE(Stack.Push(10));
    EXPECT_TRUE(Stack.IsFull());
    EXPECT_FALSE(Stack.Push(20));

    int Value = 0;
    EXPECT_TRUE(Stack.Pop(Value));
    EXPECT_EQ(10, Value);

    EXPECT_TRUE(Stack.IsEmpty());

    EXPECT_TRUE(Stack.Push(30));
    EXPECT_TRUE(Stack.Pop(Value));
    EXPECT_EQ(30, Value);
}

TEST_F(StackTest, FixedStackPeekAllowsMutation)
{
    Core::FixedStack<int, 2> Stack;

    EXPECT_TRUE(Stack.Push(10));

    int* ValuePtr = Stack.Peek();
    ASSERT_NE(nullptr, ValuePtr);

    *ValuePtr = 99;

    int Value = 0;
    EXPECT_TRUE(Stack.Pop(Value));
    EXPECT_EQ(99, Value);
}

TEST_F(StackTest, FixedStackCanEmplaceNonDefaultConstructibleValues)
{
    Core::FixedStack<FStackMoveOnly, 2> Stack;

    EXPECT_TRUE(Stack.Emplace(10));
    EXPECT_TRUE(Stack.Emplace(20));
    EXPECT_TRUE(Stack.IsFull());

    FStackMoveOnly Value(0);
    EXPECT_TRUE(Stack.Pop(Value));
    EXPECT_EQ(20, Value.Value);
    EXPECT_TRUE(Stack.Pop(Value));
    EXPECT_EQ(10, Value.Value);
    EXPECT_TRUE(Stack.IsEmpty());
}

TEST_F(StackTest, FixedStackDestroysConstructedSlots)
{
    FStackTrackedObject::Reset();

    {
        Core::FixedStack<FStackTrackedObject, 3> Stack;

        EXPECT_TRUE(Stack.Emplace(10));
        EXPECT_TRUE(Stack.Emplace(20));
        EXPECT_EQ(2, FStackTrackedObject::ConstructCount);
        EXPECT_EQ(0, FStackTrackedObject::DestructCount);

        Stack.Pop();

        EXPECT_EQ(2, FStackTrackedObject::ConstructCount);
        EXPECT_EQ(1, FStackTrackedObject::DestructCount);

        Stack.Clear();

        EXPECT_EQ(2, FStackTrackedObject::ConstructCount);
        EXPECT_EQ(2, FStackTrackedObject::DestructCount);
    }

    EXPECT_EQ(FStackTrackedObject::ConstructCount, FStackTrackedObject::DestructCount);
}

TEST_F(StackTest, FixedStackPopWithOutputDestroysConstructedSlot)
{
    FStackTrackedObject::Reset();

    {
        Core::FixedStack<FStackTrackedObject, 2> Stack;

        EXPECT_TRUE(Stack.Emplace(10));
        EXPECT_EQ(1, FStackTrackedObject::ConstructCount);
        EXPECT_EQ(0, FStackTrackedObject::DestructCount);

        FStackTrackedObject Value(0);

        EXPECT_TRUE(Stack.Pop(Value));

        EXPECT_EQ(2, FStackTrackedObject::ConstructCount);
        EXPECT_EQ(1, FStackTrackedObject::DestructCount);
    }

    EXPECT_EQ(FStackTrackedObject::ConstructCount, FStackTrackedObject::DestructCount);
}

TEST_F(StackTest, FixedStackFailedPushDoesNotModifyStack)
{
    Core::FixedStack<int, 2> Stack;

    EXPECT_TRUE(Stack.Push(10));
    EXPECT_TRUE(Stack.Push(20));

    EXPECT_FALSE(Stack.Push(30));
    EXPECT_EQ(2, Stack.GetSize());

    int Value = 0;
    EXPECT_TRUE(Stack.Pop(Value));
    EXPECT_EQ(20, Value);
    EXPECT_TRUE(Stack.Pop(Value));
    EXPECT_EQ(10, Value);
    EXPECT_TRUE(Stack.IsEmpty());
}
