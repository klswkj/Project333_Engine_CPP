# Project333 엔진코드 샘플

## 한 줄 소개

Project333은 C++20 기반으로 게임 엔진의 하위 시스템을 직접 구현하는 프로젝트입니다. 
Memory Allocator, Allocator policy 기반 Custom Container, Lock-free queue, fixed slot pool, Win32 application layer, D3D12 RHI 구조를 단계적으로 설계하고 있습니다.

## 프로젝트 목표

이 포트폴리오는 엔진 내부에서 데이터가 어떻게 할당되고, 생성되고, 이동하고, 동기화되는지를 직접 이해하고 구현하는 것입니다.

중점적으로 다룬 주제는 다음과 같습니다.

- 엔진 전용 메모리 시스템과 커스텀 할당기
- STL에만 의존하지 않는 컨테이너 설계
- 객체 생명주기와 Allocator Policy 분리
- 멀티스레드 환경을 위한 producer/consumer queue
- 고정 용량 Pool을 통한 안정적인 slot/index 기반 D3D12 CPU-GPU 리소스 관리
- Win32 application abstraction
- D3D12 backend를 숨기기 위한 RHI 구조
- GoogleTest 기반 저수준 시스템 검증

## 기술 스택

- C++20
- CMake
- GoogleTest
- Win32 API
- D3D12 / DXGI
- MSVC / Visual Studio

## 샘플코드 전체 구조

```text
Project333
├─ Source
│  ├─ Core
│  │  ├─ Memory
│  │  │  ├─ Allocator
│  │  │  └─ Container
│  │  ├─ Threading
│  │  ├─ Logging
│  │  └─ Diagnostics
│  ├─ Engine
│  │  ├─ Application
│  │  │  └─ Platform / Win32
│  │  └─ Graphics
│  │     ├─ RHI
│  │     └─ D3D12
│  └─ Main.cpp
│
├─ Tests
│  └─ ...
│
├─ Game
│  └─ ...
│
└─ GraphicsSandbox
   └─ ...
```

## 구현 상태 요약

| 영역               | 상태   | 설명                                                  |
| ------------------ | ------ | ----------------------------------------------------- |
| Core memory system | 구현   | 전역 memory facade와 allocator 교체 구조              |
| HeapAllocator      | 구현   | intrusive block metadata와 RB-tree 기반 free block 관리 |
| Custom containers  | 구현   | Vector, Array, Queue, Stack, FixedSlotPool            |
| Lock-free queue    | 구현   | SPSC, MPSC, SPMC bounded queue                        |
| Threading          | 구현   | Win32 CriticalSection wrapper                         |
| Tests              | 구현   | GoogleTest 기반 Core system 검증                      |
| Application layer  | 진행 중 | Win32 platform application과 event 전달 구조          |
| RHI layer          | 진행 중 | Device, Factory, SwapChain interface 설계             |
| D3D12 backend      | 진행 중 | Device/CommandQueue/Fence 흐름 구현 중  

## 핵심 구현

### 1. Memory System과 HeapAllocator

`Core::Memory`는 엔진 코드가 allocator 구현에 직접 의존하지 않도록 만든 공통 memory entry point입니다.
`HeapAllocator`는 고정된 heap 영역을 관리하며, 각 block에 
`BlockHeader`를 붙여 block 크기, free 여부, 이전/다음 물리 block, RB-tree node 정보를 추적합니다. 
Free block은 크기 기준 RB-tree에 등록해 best-fit 계열 탐색을 수행합니다.

주요 구현:

- intrusive `BlockHeader` 기반 heap block 관리
- physical block link를 통한 split/coalesce 처리
- RB-tree 기반 free block lookup
- canary를 이용한 free/used block corruption 검사
- double free check
- `ValidateHeap()`를 통한 heap 구조 검증
- allocation/deallocation/failure/fragmentation 관련 debug counter

관련 파일:

- `Source/Core/Public/Memory/Memory.h`
- `Source/Core/Private/Memory/Memory.cpp`
- `Source/Core/Public/Memory/Allocator/HeapAllocator.h`
- `Source/Core/Private/Memory/Allocator/HeapAllocator.cpp`

### 2. Custom Container와 Allocator Policy

엔진 내부 자료구조가 메모리 정책을 직접 선택할 수 있도록 allocator policy 기반 컨테이너를 구현했습니다.

구현한 컨테이너:

- `Core::Vector<T, AllocatorType>`
- `Core::Array<T, Size>`
- `Core::Queue<T>`
- `Core::Stack<T>`
- `Core::FixedSlotPool<T, Capacity>`

`Vector`는 memory allocation과 object construction을 분리합니다. 
Allocator는 raw storage만 제공하고, 실제 원소는 placement new로 생성합니다. 
제거 시에는 Allocator와 Element 타입 특성에 따라 destructor 호출을 제어합니다.

`InlineAllocator`는 작은 고정 용량 데이터에 대해 inline storage를 먼저 사용하고, 
필요 시 내부에서 자동으로 secondary allocator로 넘어갈 수 있도록 설계했습니다.

관련 파일:

- `Source/Core/Public/Memory/Container/Vector.h`
- `Source/Core/Public/Memory/Container/ContainerPolicies.h`
- `Source/Core/Public/Memory/Container/Queue.h`
- `Source/Core/Public/Memory/Container/Stack.h`

### 3. FixedSlotPool

`FixedSlotPool<T, Capacity>`는 고정된 slot 배열 위에서 객체를 생성하고 해제하는 pool입니다.
런타임에 반복적으로 생성/삭제되는 CPU-GPU 메모리객체를 안정적인 index 기반으로 관리하기 위한 구조입니다.

구현 특징:

- preallocated storage 위에 placement new로 객체 생성
- slot index 기반 acquire/release
- released slot 재사용
- non-default-constructible, non-movable type 지원
- atomic slot state 관리
- `WaitUntilEmpty()`를 통한 outstanding object 대기
- tagged free-list head로 ABA 위험 완화
- concurrent acquire/release stress test 작성

관련 파일:

- `Source/Core/Public/Memory/Container/FixedSlotPool.h`
- `Tests/TestFixedSlotPool.cpp`

### 4. Lock-Free Queue

멀티스레드 producer/consumer 패턴을 명확히 분리하기 위해 bounded lock-free queue를 구현했습니다.

구현한 Container:

- `SPSCQueue<T, Capacity>`:   single producer  /   single consumer
- `MPSCQueue<T, Capacity>`: multiple producers /   single consumer
- `SPMCQueue<T, Capacity>`:   single producer  / multiple consumers

구현 특징:

- runtime dynamic-allocation 없이 fixed capacity 사용
- acquire/release memory ordering 적용
- per-slot sequence 기반 상태 판별
- producer/consumer hot index cache-line alignment
- stress test에서 중복 소비, 누락, invalid value, 순서 위반 검증

관련 파일:

- `Source/Core/Public/Memory/Container/LockFreeQueue.h`
- `Tests/TestLockFreeQueue.cpp`

### 5. Application / Platform Layer

Engine layer에서는 플랫폼별 애플리케이션을 `IPlatformApplication` interface 뒤에 숨기는 구조를 만들고 있습니다.

현재 방향:

- `Application`이 여러 platform application을 소유할 수 있는 구조
- Win32 구현체를 통한 window/message pump 처리
- resize, keyboard, mouse event를 공통 interface로 전달
- update/render/present/cleanup frame 단계를 분리해 render loop 연결 준비

관련 파일:

- `Source/Engine/Public/Application/Application.h`
- `Source/Engine/Private/Application/Application.cpp`
- `Source/Engine/Public/Application/Platform/IPlatformApplication.h`
- `Source/Engine/Public/Application/Platform/Win32/Win32PlatformApplication.h`
- `Source/Engine/Private/Application/Platform/Win32/Win32PlatformApplication.cpp`

### 6. RHI / D3D12 설계

Rendering layer는 D3D12 세부 구현을 상위 엔진 코드에서 직접 알지 않아도 되도록 RHI 형태로 분리하고 있습니다.

RHI에서 정의한 개념:

- backend type
- device initialize/shutdown
- swap chain create/destroy
- begin frame / end frame
- clear back buffer
- queue type
- clear color
- swap chain descriptor

D3D12 backend는 현재 device와 command queue 흐름을 구현 중입니다.
완성된 renderer라고 포장하기보다는, 엔진 구조상 D3D12를 어디에 격리하고 어떤 interface로 상위 계층과 연결할지 설계 중인 단계로 설명하는 것이 정확합니다.

관련 파일:

- `Source/Engine/Public/Graphics/RHI/RHICommon.h`
- `Source/Engine/Public/Graphics/RHI/RHIDevice.h`
- `Source/Engine/Public/Graphics/RHI/RHIFactory.h`
- `Source/Engine/Public/Graphics/RHI/RHISwapChain.h`
- `Source/Engine/Private/Graphics/D3D12/D3D12RHIDevice.h`
- `Source/Engine/Private/Graphics/D3D12/D3D12CommandQueue.h`



## 문제 해결 기록

### 1. 엔진 전체 메모리 할당 경로 통합

문제는 각 시스템이 직접 `new/delete` 또는 `malloc/free`를 사용하면 엔진 전체의 Memory Allocation 흐름을 추적하기 어렵다는 점이었습니다.
예를 들어 Container, Renderer, Platform code가 서로 다른 방식으로 메모리를 할당하면
나중에 Memory tracking, leak detection, custom allocator 교체를 붙이기 어려워집니다.

그래서 `Memory.h/.cpp`에 `Core::Memory::Allocate/Reallocate/Deallocate` entry point를 만들고, 
내부적으로 main allocator를 교체할 수 있도록 설계했습니다. 

기본 상태에서는 `SystemMalloc`을 fallback으로 사용할 수 있고, 
엔진 초기화 이후에는 `HeapAllocator` 같은 Custom Allocator를 Main allocator로 설정할 수 있는 구조입니다.

이 구조의 의미는 `Vector` 같은 custom container가 특정 allocator 구현을 직접 알 필요 없이, 
엔진의 공통 memory policy 아래에서 동작할 수 있다는 점입니다.

### 2. HeapAllocator의 RB-tree 적용과 split/coalesce 문제

처음 문제는 기본 allocator만 사용할 경우 Memory Fragmentation, Double free, Memory Corruption 같은 문제를 엔진 수준에서 직접 추적하기 어렵다는 점이었습니다.
이를 해결하기 위해 각 block에 `BlockHeader`를 붙이고, block 크기와 free 상태, 이전/다음 물리 block 정보를 추적하는 `HeapAllocator`를 구현했습니다.

추가로 free block을 단순 list로 순회하면 block 수가 늘어날수록 allocation 탐색 비용이 커질 수 있습니다. 

그래서 free block을 크기 기준 RB-tree에 등록해 best-fit block을 찾는 구조를 적용했습니다.

구현 과정에서 어려웠던 부분은 물리 메모리 순서와 검색용 tree 순서가 서로 다르다는 점입니다.

```text
Physical memory order:
[Block][Block][Block][Block]

Free block search order:
RB-tree ordered by BlockSize
```

Allocation 중 block을 split하면 남은 free block은 물리 link에 연결하면서 RB-tree에도 다시 insert해야 합니다. 
Deallocation 중 이전/다음 block과 coalesce할 때는 기존 free block을 tree에서 먼저 remove한 뒤 물리 link와 block size를 갱신해야 합니다.
이 순서가 틀리면 Dangling tree node나 깨진 Physical link가 생길 수 있기 때문에, 
`PrevPhysBlock` / `NextPhysBlock` 갱신과 RB-tree insert/remove 순서를 분리해서 관리했습니다.

또한 canary와 `ValidateHeap()`를 통해 free/used block 상태가 맞는지, 
Physical block link가 깨지지 않았는지, block size가 heap 범위를 벗어나지 않는지 검증할 수 있게 했습니다.


### 3. Vector의 object lifetime과 move/copy 처리

`Vector`는 단순히 동적 배열처럼 보이지만, 직접 구현할 때 가장 중요한 문제는 object lifetime 관리였습니다.
`int` 같은 trivial type과 `std::string`을 포함한 struct 같은 non-trivial type은 같은 방식으로 메모리를 이동시키면 안되는 문제가 있었습니다.

그래서 `Vector`는 memory allocation과 object construction을 분리했습니다.
Allocator는 raw storage를 확보하고, 원소는 placement new로 생성합니다.
제거할 때도 메모리를 바로 해제하지 않고, non-trivial destructor가 필요한 타입인지 확인해 destructor를 호출합니다.

`Reserve`, `Insert`, `Erase`에서는 기존 원소를 새 위치로 옮겨야 합니다.
이때 무조건 move를 선택하면 예외 안정성이 떨어질 수 있고, 무조건 copy를 선택하면 move-only type을 다루기 어렵습니다.
그래서 `std::is_nothrow_move_constructible`, `std::is_copy_constructible`, `std::is_move_constructible` 같은 type trait를 기준으로 이동 경로를 선택했습니다.

또한 `Insert`는 `const T&`와 `T&&` overload를 나누어 임시 객체나 move 가능한 객체를 불필요하게 copy하지 않도록 했고,
내부 구현은 공통 helper로 모아 copy/move 경로를 명확히 유지했습니다.


### 4. FixedSlotPool의 runtime state와 동시성 문제

`FixedSlotPool`은 runtime 중 반복적으로 생성/삭제되는 객체를 매번 heap allocation으로 처리하지 않기 위한 구조입니다.
고정된 slot 배열을 만들고, 객체가 필요할 때 빈 slot index를 받아 해당 위치에 placement new로 생성합니다.
해제 시에는 객체를 파괴하고 index를 다시 free-list로 돌려줍니다.

구현 중 중요한 문제는 slot 점유 상태가 단순 Debugging 정보가 아니라 Runtime 동작에 필요한 정보라는 점이었습니다.
Debug build에서만 유지되는 flag로 slot 상태를 관리하면 release/shipping build에서 double release나 비어 있는 slot 접근을 잡지 못할 수 있습니다.

그래서 slot state는 `std::atomic<ESlotState>`로 runtime에도 유지되도록 했습니다.
`Acquire`는 free slot을 `Acquiring` 상태로 예약한 뒤 객체를 생성하고 `Occupied`로 전환합니다.
`Release`는 `Occupied` 상태를 `Releasing`으로 바꾼 뒤 객체를 파괴하고 다시 `Free`로 돌립니다.

여러 thread가 동시에 acquire/release할 수 있기 때문에 free-list head에는 index와 generation을 함께 담는 tagged head 구조를 사용해 ABA 위험을 줄였습니다.
또한 `WaitUntilEmpty()`를 통해 outstanding slot이 모두 release될 때까지 기다릴 수 있게 했습니다.


### 5. Lock-free queue의 ownership 규칙 분리

엔진 내부에서는 producer/consumer 구조가 자주 생깁니다.
하지만 모든 상황을 하나의 generic concurrent queue로 처리하면 어느 thread가 slot을 소유하는지, 언제 slot을 재사용할 수 있는지가 흐려질 수 있습니다.

그래서 producer와 consumer 수에 따라 queue를 나눴습니다.

- `SPSCQueue`: producer     1개, consumer     1개
- `MPSCQueue`: producer 여러 개, consumer     1개
- `SPMCQueue`: producer     1개, consumer 여러 개

가장 까다로운 부분은 slot ownership이 이동하는 시점을 정확히 표현하는 것이었습니다.
단순히 head/tail index만 atomic으로 두면 slot이 아직 소비되지 않았는데 재사용되는 문제가 생길 수 있습니다.
이를 해결하기 위해 각 slot에 sequence 값을 두고, slot이 비어 있는지, 값이 준비됐는지, 아직 재사용하면 안 되는지를 판별하게 했습니다.

MPSC에서는 여러 producer가 같은 tail을 경쟁하므로 `compare_exchange_weak`로 slot 예약에 성공한 producer만 값을 생성합니다.
SPMC에서는 여러 consumer가 같은 head를 경쟁하므로 소비 권한을 얻은 consumer만 값을 move하고 slot을 release합니다.

테스트에서는 값 누락, 중복 소비, invalid value, producer별 순서 위반을 확인하는 stress test를 작성했습니다.

### 6. RefCntPtr을 통한 D3D12/COM 객체 수명 관리

D3D12와 COM style API는 `AddRef/Release` 기반 수명 관리를 사용합니다.
이를 raw pointer로 직접 관리하면 release 누락이나 double release 위험이 커집니다.

그래서 `Core::RefCntPtr<T>`를 구현해 reference-counted object를 RAII로 관리하도록 했습니다.
Copy 시에는 `AddRef`, destructor와 reset 시에는 `Release`가 호출되도록 하고, move 시에는 pointer ownership을 이전합니다.

또한 `GetAddressOf()`와 `ReleaseAndGetAddressOf()`를 분리했습니다.
기존 object를 유지한 채 address를 넘기는 경우와, 기존 object를 release한 뒤 새로 초기화하려는 경우의 의도를 API 이름으로 구분하기 위해서입니다.
D3D12 object 생성이나 `QueryInterface` 계열 코드를 작성할 때 ownership 실수를 줄이는 데 도움이 됩니다.


### 7. DebugCheck의 header dependency와 build mode 분리 고민

Debug check system을 만들 때 처음에는 macro 안에서 바로 logging을 호출하는 방식이 가장 간단해 보였습니다.
하지만 `DebugCheck.h`가 `CoreMinimal.h`에 포함될 가능성이 높기 때문에, 
이 header가 `Logger.h`에 직접 강하게 묶이면 Core 전체의 include dependency가 커질 수 있습니다.

현재는 `DO_CHECK`, `DO_ENSURE`를 통해 Debug/Release mode별로 check, verify, ensure 동작을 분기하고 있습니다.
`CHECK`는 Debug mode에서만 활성화하고, Release mode나 Shipping 제거하거나 expression evaluation만 남기는 식으로 code path를 줄일 수 있습니다.

향후 개선 방향은 check macro가 조건 평가와 실패 정보 전달만 담당하고, 
실제 Logging, Formatting, Debug break 처리는 cpp 구현부로 분리하는 것입니다. 
이렇게 하면 Header dependency와 Release시 Binary code size를 줄일 수 있습니다.


## 대표 코드 예시

### Allocator policy를 사용하는 Container

`Vector`는 메모리 확보와 객체 생성을 분리해서 처리합니다.
할당은 Allocator policy에 맡기고, 실제 객체는 placement new로 생성합니다.

```cpp
template <typename ElementType, typename AllocatorType = DefaultContainerAllocator<ElementType>>
requires ContainerAllocatorPolicy<AllocatorType>
class Vector final
{
public:
    template <typename... TArgs>
    [[nodiscard]]
    bool Emplace_back(TArgs&&... InputArgs)
    {
        if (false == GrowIfNeeded())
        {
            return false;
        }

        new (m_Data + m_Size) ElementType(std::forward<TArgs>(InputArgs)...);
        ++m_Size;

        return true;
    }
};
```

### HeapAllocator의 free block split

Allocation 후 남은 공간이 충분하면 새 free block을 만들고, physical link와 RB-tree를 모두 갱신합니다.

```cpp
BlockHeader* SplitRemainderBlock = reinterpret_cast<BlockHeader*>(
    reinterpret_cast<char*>(BestFitBlock) + ActualAllocationSize);

SplitRemainderBlock->BlockSize = RemainingSize;
SplitRemainderBlock->IsFree = true;

SplitRemainderBlock->NextPhysBlock = BestFitBlock->NextPhysBlock;
SplitRemainderBlock->PrevPhysBlock = BestFitBlock;

BestFitBlock->NextPhysBlock = SplitRemainderBlock;
BestFitBlock->BlockSize = ActualAllocationSize;

FreeBlocksTree.Insert(SplitRemainderBlock);
```

### FixedSlotPool의 acquire 흐름

`FixedSlotPool`은 빈 slot index를 얻은 뒤 해당 위치에 객체를 직접 생성합니다.

```cpp
template<typename... TArgs>
[[nodiscard]]
T* Acquire(size_t& OutIndex, TArgs&&... InputArgs)
{
    size_t FreeIndex = 0;

    if (false == m_FreeIndices.Pop(FreeIndex))
    {
        return nullptr;
    }

    T* Slot = GetSlot(FreeIndex);
    std::construct_at(Slot, std::forward<TArgs>(InputArgs)...);

    m_SlotStates[FreeIndex].store(ESlotState::Occupied, std::memory_order_release);
    m_OccupiedCount.fetch_add(1, std::memory_order_release);

    OutIndex = FreeIndex;
    return Slot;
}
```

### MPSC lock-free queue의 slot 예약

`MPSCQueue`는 여러 producer가 같은 tail을 경쟁할 수 있으므로 `compare_exchange_weak`로 slot을 예약합니다. 예약에 성공한 producer만 값을 생성하고 sequence를 갱신합니다.

```cpp
if (Sequence == TailIndex)
{
    if (true == m_TailIndex.compare_exchange_weak(
        TailIndex,
        TailIndex + 1,
        std::memory_order_acq_rel,
        std::memory_order_relaxed))
    {
        LockFreeQueuePrivate::ConstructValue(CurrentSlot->GetValueStorage(), InputValue);
        CurrentSlot->Sequence.store(TailIndex + 1, std::memory_order_release);

        return true;
    }
}
```

## 테스트

GoogleTest를 사용해 Core 계층의 저수준 시스템을 검증했습니다.

검증한 내용:

- HeapAllocator 기본 동작, split/coalesce, reallocate path, heap validation
- Vector의 push, insert, resize, erase, destructor 호출
- Array, Queue, Stack의 기본 자료구조 동작
- FixedSlotPool의 slot 재사용, 비이동 타입 지원, 동시 acquire/release
- MPSC/SPMC lock-free queue stress test


## 제 포트폴리오의 강조점

이 프로젝트는 게임의 결과 화면보다 엔진 내부 구현력을 보여주는 프로젝트입니다.

- C++ object lifetime
- Custom allocator
- Intrusive data structure
- RB-tree free block lookup
- RB-tree Split / Coalesce
- Lock-free programming
- Atomic memory ordering
- Fixed-capacity runtime system
- Win32 platform abstraction
- D3D12 RHI architecture
- RAII for reference-counted objects
- GoogleTest 기반 low-level system verification
