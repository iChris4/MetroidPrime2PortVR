#ifndef _RSTL_SINGLE_PTR
#define _RSTL_SINGLE_PTR

#include "types.h"

namespace rstl {
template < typename T >
class single_ptr {
  mutable T* mPtr;

public:
  single_ptr() : mPtr(nullptr) {}
  single_ptr(T* ptr) : mPtr(ptr) {}
  single_ptr(const single_ptr& other) : mPtr(other.mPtr) { other.mPtr = nullptr; }
  ~single_ptr() { delete mPtr; }
  single_ptr& operator=(single_ptr& other) {
    if (&other == this) {
      return *this;
    }
    delete mPtr;
    mPtr = other.mPtr;
    other.mPtr = nullptr;
    return *this;
  }

  single_ptr& operator=(T* const ptr) {
    delete mPtr;
    mPtr = ptr;
    return *this;
  }

  T* get() const { return mPtr; }
  // const T* get() const { return mPtr; }
  T* operator->() const { return mPtr; }
#ifdef TARGET_PC
  // single_ptr<void> (CPlayer::mScanTargetIdTextureData) is legal for MWCC; host compilers reject
  // the `void&` declaration at class instantiation, so deduce the reference type lazily.
  decltype(auto) operator*() { return *mPtr; }
  decltype(auto) operator*() const { return *mPtr; }
#else
  T& operator*() { return *mPtr; }
  const T& operator*() const { return *mPtr; }
#endif

  bool null() const { return mPtr == nullptr; }
  T* release() {
    T* ptr = mPtr;
    mPtr = nullptr;
    return ptr;
  }

  // This is certainly not real, but handy to force not-inline
  single_ptr& Set(T* ptr);
};

template < typename T >
single_ptr< T >& single_ptr< T >::Set(T* ptr) {
  return *this = ptr;
}

#ifdef TARGET_PC
// single_ptr<void> owns a raw allocation: `delete (void*)` is undefined on the host, free it.
template <>
inline single_ptr< void >::~single_ptr() {
  ::operator delete(mPtr);
}
#endif

typedef single_ptr< void > unk_singleptr;
CHECK_SIZEOF(unk_singleptr, 0x4);
} // namespace rstl

#endif // _RSTL_SINGLE_PTR
