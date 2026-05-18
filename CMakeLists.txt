# CMakeList.txt : 최상위 CMake 프로젝트 파일, 전역 구성을 수행하고
# 여기에 하위 프로젝트를 포함합니다.

cmake_minimum_required(VERSION 3.20)

project(Project333 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

# =========================
# Core
# =========================

add_library(Core)

target_sources(Core
    PRIVATE
        Source/Core/Private/Memory/Allocator/HeapAllocator.cpp
        Source/Core/Private/Logging/Logger.cpp
        Source/Core/Private/Memory/SystemMalloc.cpp
        Source/Core/Private/Memory/Memory.cpp
        Source/Core/Private/Memory/Container/Vector.cpp

    PUBLIC
        Source/Core/Public/Memory/Allocator/IAllocator.h
        Source/Core/Public/Memory/Container/Vector.h
        Source/Core/Public/Diagnostics/DebugCheck.h
        Source/Core/Public/Memory/Container/Array.h
        Source/Core/Public/Memory/MallocBase.h
        Source/Core/Public/Memory/SystemMalloc.h
        Source/Core/Public/Memory/MemoryAlignmentPolicy.h
        Source/Core/Public/Memory/Memory.h
        Source/Core/Public/Memory/Container/ContainerPolicies.h
 "Source/Core/Public/Utils/EnumClassCompare.h" "Source/Core/Public/Utils/RefCount.h" "Source/Core/Public/Memory/Container/Queue.h" "Source/Core/Public/Memory/Container/LockFreeQueue.h" "Source/Core/Public/Memory/Container/FixedSlotPool.h")

target_include_directories(Core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/Core/Public
)

if (MSVC)
    target_compile_options(Core PUBLIC /utf-8)
endif()

# Visual Studio Solution Explorer에서 실제 폴더 구조에 가깝게 보이게 함
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/Source/Core PREFIX "Core" FILES
    Source/Core/Private/Memory/Allocator/HeapAllocator.cpp
    Source/Core/Private/Logging/Logger.cpp
    Source/Core/Private/Memory/SystemMalloc.cpp
    Source/Core/Private/Memory/Memory.cpp
    Source/Core/Private/Memory/Container/Vector.cpp

    Source/Core/Public/Memory/Allocator/IAllocator.h
    Source/Core/Public/Memory/Container/Vector.h
    Source/Core/Public/Diagnostics/DebugCheck.h
    Source/Core/Public/Memory/Container/Array.h
    Source/Core/Public/Memory/MallocBase.h
    Source/Core/Public/Memory/SystemMalloc.h
    Source/Core/Public/Memory/MemoryAlignmentPolicy.h
    Source/Core/Public/Memory/Memory.h
    Source/Core/Public/Memory/Container/ContainerPolicies.h
)

# =========================
# Engine
# =========================

add_library(Engine)

target_sources(Engine
    PRIVATE
        Source/Engine/Private/Application/Application.cpp
        Source/Engine/Private/Application/Platform/Win32/Win32Window.cpp
        Source/Engine/Private/Application/Platform/Win32/Win32PlatformApplication.cpp

    PUBLIC
        Source/Engine/Public/Application/Application.h
        Source/Engine/Public/Application/Platform/IPlatformApplication.h
        Source/Engine/Public/Application/Platform/Win32/Win32Window.h
        Source/Engine/Public/Application/Platform/Win32/Win32PlatformApplication.h
 "Source/Core/Public/Utils/EnumClassCompare.h" "Source/Core/Public/Utils/RefCount.h" "Source/Core/Public/Memory/Container/Queue.h" "Source/Core/Public/Memory/Container/LockFreeQueue.h" "Source/Core/Public/Memory/Container/FixedSlotPool.h")

target_include_directories(Engine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/Engine/Public
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/Engine/Private
)

target_link_libraries(Engine
    PUBLIC
        Core

    PRIVATE
        d3d12
        dxgi
        dxguid
)

target_link_libraries(Game
    PUBLIC
        Engine
)

target_link_libraries(GamePrototype
    PUBLIC
        Engine
)

target_link_libraries(GraphicsSandbox
    PUBLIC
        Engine
)

if (MSVC)
    target_compile_options(Engine PUBLIC /utf-8)
endif()

# Visual Studio Solution Explorer에서 실제 폴더 구조에 가깝게 보이게 함
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/Source/Engine PREFIX "Engine" FILES
    Source/Engine/Private/Application/Application.cpp
    Source/Engine/Private/Application/Platform/Win32/Win32Window.cpp
    Source/Engine/Private/Application/Platform/Win32/Win32PlatformApplication.cpp

    Source/Engine/Public/Application/Application.h
    Source/Engine/Public/Application/Platform/IPlatformApplication.h
    Source/Engine/Public/Application/Platform/Win32/Win32Window.h
    Source/Engine/Public/Application/Platform/Win32/Win32PlatformApplication.h
)

# =========================
# Project333 executable
# =========================

add_executable(Project333
    Source/Main.cpp

 "Source/Core/Public/Utils/EnumClassCompare.h" "Source/Core/Public/Utils/RefCount.h" "Source/Core/Public/Memory/Container/Queue.h" "Source/Core/Public/Memory/Container/LockFreeQueue.h" "Source/Core/Public/Memory/Container/FixedSlotPool.h")

target_link_libraries(Project333
    PRIVATE
        Game
)

if (MSVC)
    target_compile_options(Project333 PRIVATE /utf-8)
endif()

# =========================
# Tests
# =========================

add_subdirectory(Tests)
