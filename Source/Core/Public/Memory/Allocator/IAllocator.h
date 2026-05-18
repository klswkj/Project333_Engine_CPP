#pragma once

namespace Core
{
    class IAllocator
    {
    public:
        virtual ~IAllocator() = default;

        virtual void* Allocate(size_t Size, size_t Alignment /*= eDefaultAlignment*/) = 0;
        virtual void  Deallocate(void* Ptr) = 0;
    };
}

/*
FMalloc은 인터페이스고, 실제 allocator 구현체는 보통 이런 쪽에 있어.

Engine/Source/Runtime/Core/Private/HAL/MallocBinned.cpp
Engine/Source/Runtime/Core/Private/HAL/MallocBinned2.cpp
Engine/Source/Runtime/Core/Private/HAL/MallocBinned3.cpp
Engine/Source/Runtime/Core/Private/HAL/MallocAnsi.cpp
Engine/Source/Runtime/Core/Private/HAL/MallocMimalloc.cpp

네 HeapAllocator.cpp랑 대응되는 건 이쪽이야.

HeapAllocator.cpp ≈ MallocBinned2.cpp / MallocAnsi.cpp / MallocMimalloc.cpp

물론 Unreal의 allocator는 훨씬 복잡하고, 
멀티스레드/통계/플랫폼/정렬/디버그 추적까지 들어가 있어서 그대로 따라 하려고 하면 너무 무거워.

===================================================

GMalloc은 Unreal의 “주요 전역 allocator 포인터”가 맞다.
하지만 “Unreal에 allocator가 GMalloc 하나뿐이다”라고 이해하면 조금 틀려. 더 정확히는:

GMalloc은 일반 heap allocation의 기본 전역 관문이고,
그 뒤에 실제 allocator 구현체 하나가 연결된다.
하지만 Unreal은 그 위아래로 여러 allocator policy와 특수 메모리 시스템을 같이 쓴다.

네 프로젝트에서는 일단 GMainMalloc 또는 GAllocator 하나를 두고 
Vector, Array가 전부 거기를 타게 만드는 게 지금 단계에서는 충분히 좋은 구조야.

======================================================================


FMalloc을 상속한 클래스들
FMallocAnsi     - Windows, Linux, MacOS 등에서 기본 C 런타임 malloc/free를 래핑한 구현체
FMallocBinned   - UE4 초반까지, UE5 초반까지도 쓰이는 경우가 있음
FMallocBinned2  - 개선판 UE4 후반, UE5 초반까지
FMallocBinned3  - 가장최신
FMallocMimalloc - mimalloc 라이브러리를 래핑한 구현체, UE5.1부터
FMallocJemalloc - jemalloc 라이브러리를 래핑한 구현체, UE5.1부터
FMallocTBB      - Intel TBB 라이브러리를 래핑한 구현체, UE5.1부터

2. 디버그 / 검증 / 프록시 allocator 계열
FMallocStomp                       - 할당된 메모리를 해제한 후에 접근하면 의도적으로 크래시를 일으키는
                                     메모리 오버런/use-after-free 같은 문제를 잡기 위한 allocator, 디버그용
FMallocStomp2                      - FMallocStomp의 개선판, UE5.1부터
FMallocPoisonProxy                 - 다른 allocator를 감싸서 poison/fill 같은 검증 처리를 하는 proxy allocator
FMallocVerify / FMallocVerifyProxy - allocation/free 검증용 proxy 계열. 버전마다 이름/구조가 다를 수 있음.
FMallocLeakDetection               - 메모리 누수 감지용 allocator, UE5.1부터, low-level memory leak 추적용
FMallocProfiler                    - MemoryProfiler2같은 메모리 프로파일러용
FMallocCallstackHandler            - 할당/해제 시점의 콜스택을 캡처해서 추적하는 allocator, 디버그용

나는
1단계: 기본 allocator
- MallocBase
- HeapAllocator
- Memory::Allocate / Deallocate
- GMainMalloc

2단계: 디버그 allocator
- DebugMallocProxy
- PoisonMallocProxy
- StompMalloc 비슷한 것

3단계: 성능 allocator
- BinnedAllocator
- PoolAllocator
- FrameAllocator

이렇게

=============================

HeapAllocator는 Unreal 기준으로 보면 FMallocAnsi보다는 
한 단계 더 엔진 allocator에 가깝고, FMallocBinned보다는 아직 단순한 위치

==============================

가장 직접적으로 참고할 클래스는 이 4개면 충분해.

FMalloc
FMallocAnsi
FMallocBinned2 또는 FMallocBinned3
FMallocPoisonProxy
즉 네 구조로 바꾸면:

FMalloc                  → MallocBase
FMallocAnsi              → SystemMalloc
FMallocBinned2/3          → 나중에 만들 BinnedAllocator
FMallocPoisonProxy        → DebugAllocatorProxy
GMalloc                  → GMainMalloc
FMemory::Allocate/Deallocate      → Memory::Allocate/Free


*/