#pragma once

#include <type_traits>
#include <concepts>
#include <cstddef>

#include "../Logging/Logger.h"
#include "../Diagnostics/DebugCheck.h"

#define REF_COUNT_NODISCARD [[nodiscard("This func transfers ownership. Ignoring the returned pointer leaks the reference.")]]

namespace Core
{
	template<typename T>
	concept CHasGetRefCount = requires(T * Ptr)
	{
		{ Ptr->GetRefCount() } -> std::convertible_to<unsigned long>;
	};

	template<typename T>
	concept CHasAddRefReleaseReturningCount = requires(T * Ptr)
	{
		{ Ptr->AddRef() } -> std::convertible_to<unsigned long>;
		{ Ptr->Release() } -> std::convertible_to<unsigned long>;
	};

	// ReferencedType -> T
	// ReferenceType -> TPtr
	template<typename T>
	class RefCntPtr
	{
	private:
		template <typename OtherType> friend class RefCntPtr;

		typedef T* Tptr;

		T* m_Ptr;

	public:

		RefCntPtr() : m_Ptr(nullptr) {}
		RefCntPtr(std::nullptr_t) : m_Ptr(nullptr) {}

		RefCntPtr(T* InReference, bool bAddRef = true)
			: m_Ptr(InReference)
		{
			InternalAddRef(bAddRef);
		}

		RefCntPtr(const RefCntPtr& Other)
			: m_Ptr(Other.m_Ptr)
		{
			InternalAddRef();
		}

		~RefCntPtr()
		{
			InternalRelease();
		}

		template<typename CopyReferencedType>
			requires std::is_convertible_v<CopyReferencedType*, T*>
		explicit RefCntPtr(const RefCntPtr<CopyReferencedType>& Copy)
			: m_Ptr(Copy.m_Ptr)
		{
			InternalAddRef();
		}

		RefCntPtr(RefCntPtr&& Move) noexcept
		{
			m_Ptr = Move.m_Ptr;
			Move.m_Ptr = nullptr;
		}

		template<typename MoveReferencedType>
			requires std::is_convertible_v<MoveReferencedType*, T*>
		explicit RefCntPtr(RefCntPtr<MoveReferencedType>&& Move) noexcept
			: m_Ptr(Move.m_Ptr)
		{
			Move.m_Ptr = nullptr;
		}

		RefCntPtr& operator=(std::nullptr_t)
		{
			Reset();

			return *this;
		}

		RefCntPtr& operator=(T* InReference)
		{
			if (nullptr != InReference)
			{
				InReference->AddRef();
			}

			T* OldReference = m_Ptr;

			m_Ptr = InReference;

			if (nullptr != OldReference)
			{
				OldReference->Release();
			}

			return *this;
		}

		RefCntPtr& operator=(const RefCntPtr& InPtr)
		{
			return *this = InPtr.m_Ptr;
		}

		template<typename CopyReferencedType>
			requires std::is_convertible_v<CopyReferencedType*, T*>
		RefCntPtr& operator=(const RefCntPtr<CopyReferencedType>& InPtr)
		{
			return *this = InPtr.GetReference();
		}

		RefCntPtr& operator=(RefCntPtr&& InPtr) noexcept
		{
			if (this != &InPtr)
			{
				T* OldReference = m_Ptr;
				m_Ptr = InPtr.m_Ptr;
				InPtr.m_Ptr = nullptr;
				if (OldReference)
				{
					OldReference->Release();
				}
			}
			return *this;
		}

		// 이 코드의 역은
		// COM객체의 QueryInterface()
		// ex) SwapChain3 = std::move(SwapChain1) -> 불가능
		//     HRESULT Result = SwapChain1->QueryInterface(IID_PPV_ARGS(SwapChain3.GetAddressOf())
		//     ㄴ 가능
		template<typename MoveReferencedType>
			requires std::is_convertible_v<MoveReferencedType*, T*>
		RefCntPtr& operator=(RefCntPtr<MoveReferencedType>&& InPtr) noexcept
		{
			// InPtr is a different type (or we would have called the other operator), so we need not test &InPtr != this
			T* OldReference = m_Ptr;
			m_Ptr = InPtr.m_Ptr;
			InPtr.m_Ptr = nullptr;

			if (OldReference)
			{
				OldReference->Release();
			}
			return *this;
		}


		T** GetInitReference()
		{
			*this = nullptr;
			return &m_Ptr;
		}

		friend bool IsValidRef(const RefCntPtr& InReference)
		{
			return InReference.m_Ptr != nullptr;
		}

		void SafeRelease()
		{
			*this = nullptr;
		}

		unsigned long GetRefCount() const
		{
			unsigned long Result = 0;

			if (nullptr == m_Ptr)
			{
				return Result;
			}

			if constexpr (CHasGetRefCount<T>)
			{
				Result = static_cast<unsigned long>(m_Ptr->GetRefCount());
			}
			else if constexpr (CHasAddRefReleaseReturningCount<T>)
			{
				Result = static_cast<unsigned long>(m_Ptr->AddRef());
				Result = static_cast<unsigned long>(m_Ptr->Release());
			}
			else
			{
				static_assert
					(
						CHasGetRefCount<T> || CHasAddRefReleaseReturningCount<T>,
						"T must provide GetRefCount(), or AddRef()/Release() must return a reference count."
						);
			}

			return Result;
		}

		void Swap(RefCntPtr& InPtr) // this does not change the reference count, and so is faster
		{
			T* OldReference = m_Ptr;

			m_Ptr = InPtr.m_Ptr;
			InPtr.m_Ptr = OldReference;
		}

		void Reset()
		{
			InternalRelease();
		}

		void Attach(T* InPtr)
		{
			if (m_Ptr == InPtr)
			{
				return;
			}

			InternalRelease();

			m_Ptr = InPtr;
		}

		REF_COUNT_NODISCARD
			T* Detach()
		{
			T* DetachedPtr = m_Ptr;

			m_Ptr = nullptr;

			return DetachedPtr;
		}

	public:
		bool operator==(const RefCntPtr& B) const { return GetReference() == B.GetReference(); }
		bool operator==(T* B)               const { return GetReference() == B; }

		T* operator->() const { return  m_Ptr; }
		T& operator*()  const { return *m_Ptr; }

		T* Get()        const { return m_Ptr; }

		T* GetReference() const { return m_Ptr; }

		T** GetAddressOf() { return &m_Ptr; }
		T* const* GetAddressOf() const { return &m_Ptr; }

		REF_COUNT_NODISCARD T** ReleaseAndGetAddressOf() { InternalRelease(); return &m_Ptr; }

		explicit operator bool() const { return nullptr != m_Ptr; }

		bool IsValid() const { return nullptr != m_Ptr; }

	private:
		void InternalAddRef(bool bAddRef = true)
		{
			if (nullptr != m_Ptr && true == bAddRef)
			{
				m_Ptr->AddRef();
			}
		}

		void InternalRelease()
		{
			T* PtrToRelease = m_Ptr;

			if (nullptr != PtrToRelease)
			{
				m_Ptr = nullptr;

				PtrToRelease->Release();
			}
		}
	};
}

#undef REF_COUNT_NODISCARD 