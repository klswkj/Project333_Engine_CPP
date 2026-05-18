![Project333 Loading Animation](Project333_Loading_Animation_Orbit_Fixed_v2.gif)

# Project333

> C++20 engine systems portfolio sample.

Project333 is a C++20 game-engine foundation project focused on low-level engine systems: memory allocation, custom containers, thread communication primitives, a Win32 application layer, and an in-progress D3D12 RHI layer.

The project is built close to the metal. Instead of treating allocation, object lifetime, synchronization, and platform/rendering boundaries as hidden details, it makes them explicit and testable.

## Project Snapshot

| Area | Status | Notes |
| --- | --- | --- |
| Core memory system | Implemented | Global memory facade with pluggable allocator backend |
| Heap allocator | Implemented | Best-fit style allocator with intrusive block metadata and RB-tree free block lookup |
| Custom containers | Implemented | `Vector`, `Array`, `Queue`, `Stack`, `FixedSlotPool`, lock-free queue variants |
| Threading primitive | Implemented | Win32 critical section wrapper |
| Application layer | In progress | Platform-agnostic application loop backed by Win32 application/window classes |
| RHI abstraction | In progress | Backend-facing interfaces for device, factory, swap chain, frame control, and clear commands |
| D3D12 backend | In progress | Device and command queue scaffolding, fence/queue flow under development |
| Tests | Implemented for Core systems | GoogleTest coverage for allocator, containers, slot pool, and lock-free queues |

## Why I Built This

This project started as a way to understand engine architecture from the bottom up. The goal is not only to make features work, but to understand how an engine owns memory, controls object lifetime, separates platform-specific code, and verifies low-level behavior.

The main engineering questions behind the project are:

- How should engine containers allocate memory without depending directly on `std::allocator`?
- How can allocation paths be unified so tracking, profiling, or custom allocators can be added later?
- How should fixed-capacity systems avoid runtime allocation after initialization?
- How can producer/consumer communication be modeled with clear ownership rules?
- How should a renderer-facing interface hide platform-specific D3D12 details?
- How can low-level code stay testable while still using platform APIs?

## Core Architecture

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
├─ Tests
│  ├─ TestHeapAllocator.cpp
│  ├─ TestVector.cpp
│  ├─ TestArray.cpp
│  ├─ TestQueue.cpp
│  ├─ TestStack.cpp
│  ├─ TestLockFreeQueue.cpp
│  └─ TestFixedSlotPool.cpp
│
├─ Game
│  └─ ...
└─ GraphicsSandbox
   └─ ...
```

## Key Implementations

### 1. Memory System and Heap Allocator

The Core memory module exposes a global memory facade:

- `Core::Memory::Initialize()`
- `Core::Memory::SetMainAllocator()`
- `Core::Memory::Allocate()`
- `Core::Memory::Reallocate()`
- `Core::Memory::Deallocate()`

Behind that interface, `HeapAllocator` manages a fixed heap region with block headers, physical block links, and RB-tree based free block lookup. The allocator is designed around engine-style control over memory behavior rather than relying on general-purpose allocation everywhere.

Notable details:

- intrusive `BlockHeader` metadata
- physical block links for neighboring block management
- RB-tree free block tracking
- split/coalesce behavior for allocation and deallocation
- canary checks for free/used block corruption detection
- debug counters for allocation, deallocation, failed allocation, and fragmentation-related data
- custom allocator integration through container allocation policies

Relevant files:

- `Source/Core/Public/Memory/Memory.h`
- `Source/Core/Private/Memory/Memory.cpp`
- `Source/Core/Public/Memory/Allocator/HeapAllocator.h`
- `Source/Core/Private/Memory/Allocator/HeapAllocator.cpp`

### 2. Custom Container Layer

The project includes engine-owned containers instead of relying only on STL containers. The containers use allocator policies so memory behavior can be swapped or specialized.

Implemented containers include:

- `Core::Vector<T, AllocatorType>`
- `Core::Array<T, Size>`
- `Core::Queue<T>`
- `Core::Stack<T>`
- `Core::FixedSlotPool<T, Capacity>`

`Vector` separates raw memory allocation from object construction. It uses placement-new construction, explicit destruction, move/copy selection based on type traits, and allocator policies.

`InlineAllocator` provides inline storage for fixed-capacity use cases, with fallback support through a secondary allocator.

Relevant files:

- `Source/Core/Public/Memory/Container/Vector.h`
- `Source/Core/Public/Memory/Container/Array.h`
- `Source/Core/Public/Memory/Container/Queue.h`
- `Source/Core/Public/Memory/Container/Stack.h`
- `Source/Core/Public/Memory/Container/FixedSlotPool.h`
- `Source/Core/Public/Memory/Container/ContainerPolicies.h`

### 3. Fixed Slot Pool

`FixedSlotPool<T, Capacity>` is a fixed-capacity object pool for systems that need stable indexed slots and controlled object lifetime.

It supports:

- construction directly into preallocated slots
- release by slot index
- reuse of released slots
- non-default-constructible and non-movable types
- atomic runtime slot states
- concurrent acquire/release stress testing
- `WaitUntilEmpty()` synchronization using atomic wait/notify
- tagged free-list head to reduce ABA risk in the free index stack

Relevant files:

- `Source/Core/Public/Memory/Container/FixedSlotPool.h`
- `Tests/TestFixedSlotPool.cpp`

### 4. Lock-Free Queue Variants

The project includes bounded lock-free queue primitives for different producer/consumer ownership patterns:

- `SPSCQueue<T, Capacity>`: single producer, single consumer
- `MPSCQueue<T, Capacity>`: multiple producers, single consumer
- `SPMCQueue<T, Capacity>`: single producer, multiple consumers

The queues use atomics, acquire/release memory ordering, per-slot sequence values, cache-line alignment for hot indices, and bounded capacity to avoid dynamic allocation during runtime.

Relevant files:

- `Source/Core/Public/Memory/Container/LockFreeQueue.h`
- `Tests/TestLockFreeQueue.cpp`

### 5. Platform, RHI, and D3D12 Direction

The Engine layer introduces an application abstraction through `IPlatformApplication`, with Win32 as the current platform implementation.

The rendering layer is being shaped around a Render Hardware Interface. The public RHI layer defines backend-neutral concepts such as device initialization, swap chain creation, frame begin/end, and back buffer clear. The D3D12 backend currently contains scaffolding for device and command queue ownership.

Relevant files:

- `Source/Engine/Public/Application/Application.h`
- `Source/Engine/Public/Application/Platform/IPlatformApplication.h`
- `Source/Engine/Public/Graphics/RHI/RHICommon.h`
- `Source/Engine/Public/Graphics/RHI/RHIDevice.h`
- `Source/Engine/Private/Graphics/D3D12/D3D12RHIDevice.h`
- `Source/Engine/Private/Graphics/D3D12/D3D12CommandQueue.h`

## Engineering Challenges

### Unified Memory Allocation

Direct `new/delete` or `malloc/free` calls scattered across engine systems make allocator replacement, memory tracking, and leak detection harder. Project333 routes container allocation through `Core::Memory`, so the actual allocator can be changed behind a single entry point.

### RB-Tree Free Block Lookup

A simple free-list scan becomes more expensive as the number of blocks grows. `HeapAllocator` keeps physical block order for split/coalesce operations, while registering free blocks in an RB-tree for best-fit lookup. This separates the physical memory layout from the search structure.

### Manual Object Lifetime in Containers

Custom containers cannot treat every element as raw bytes. `Vector` explicitly separates raw storage allocation from object construction/destruction and chooses move or copy paths based on type traits and `noexcept` move availability.

### Lock-Free Ownership Rules

Instead of one generic concurrent queue, the project separates SPSC, MPSC, and SPMC queues. Each queue has a narrower ownership model, and per-slot sequence values control when a slot is ready, consumed, or reusable.

### Reference-Counted Object Management

D3D12 and COM-style APIs rely on explicit reference counting. `Core::RefCntPtr` wraps `AddRef/Release` in RAII and separates `GetAddressOf()` from `ReleaseAndGetAddressOf()` to make initialization ownership clearer.

## Testing

The test suite uses GoogleTest and focuses on lower-level systems where regressions are easiest to miss.

Covered areas:

- heap allocator behavior, validation, split/coalesce cases, and reallocation paths
- vector construction, resizing, insertion, erasing, and destruction behavior
- array, queue, and stack behavior
- fixed slot pool reuse and concurrent acquire/release
- MPSC and SPMC lock-free queue stress cases

Example test targets:

```bash
cmake --build build
ctest --test-dir build
```

## Tech Stack

- C++20
- CMake
- GoogleTest
- Win32 API
- D3D12 / DXGI
- MSVC / Visual Studio

## What This Project Demonstrates

- Manual memory management and allocator design
- Type-trait based object lifetime handling
- Custom container implementation
- Lock-free data structure design using atomics
- Fixed-capacity runtime systems
- Platform abstraction for Win32 applications
- Early-stage graphics backend architecture
- Test-driven verification of low-level systems

## Current Focus

The project is currently focused on strengthening the engine foundation before adding higher-level rendering and gameplay features.

Near-term work:

- complete D3D12 device initialization flow
- finish command queue fence/wait behavior
- connect swap chain creation to the Win32 window layer
- clean up CMake source grouping as modules grow
- expand tests for allocator edge cases and application lifecycle behavior
