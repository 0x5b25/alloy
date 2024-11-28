#pragma once
/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#include "veldrid/common/Macros.h"
//#include "include/private/Templates.h"

#include <atomic>       // std::atomic, std::memory_order_*
#include <cstddef>      // std::nullptr_t
#include <iosfwd>       // std::basic_ostream
#include <memory>       // TODO: unused
#include <type_traits>  // std::enable_if, std::is_convertible
#include <utility>      // std::forward, std::swap
#include <cassert>

namespace Veldrid{
/** \class RefCntBase

    RefCntBase is the base class for objects that may be shared by multiple
    objects. When an existing owner wants to share a reference, it calls ref().
    When an owner wants to release its reference, it calls unref(). When the
    shared object's reference count goes to zero as the result of an unref()
    call, its (virtual) destructor is called. It is an error for the
    destructor to be called explicitly (or via the object going out of scope on
    the stack or calling delete) if getRefCnt() > 1.
*/
class VLD_API RefCntBase {
public:
    /** Default construct, initializing the reference count to 1.
    */
    RefCntBase() : fRefCnt(1) {}

    /** Destruct, asserting that the reference count is 1.
    */
    virtual ~RefCntBase() {
    #ifdef VLD_DEBUG
        assert(this->getRefCnt() == 1);
        // illegal value, to catch us if we reuse after delete
        fRefCnt.store(0, std::memory_order_relaxed);
    #endif
    }

    /** May return true if the caller is the only owner.
     *  Ensures that all previous owner's actions are complete.
     */
    bool unique() const {
        if (1 == fRefCnt.load(std::memory_order_acquire)) {
            // The acquire barrier is only really needed if we return true.  It
            // prevents code conditioned on the result of unique() from running
            // until previous owners are all totally done calling unref().
            return true;
        }
        return false;
    }

    /** Increment the reference count. Must be balanced by a call to unref().
    */
    void ref() const {
        assert(this->getRefCnt() > 0);
        // No barrier required.
        (void)fRefCnt.fetch_add(+1, std::memory_order_relaxed);
    }

    /** Decrement the reference count. If the reference count is 1 before the
        decrement, then delete the object. Note that if this is the case, then
        the object needs to have been allocated via new, and not on the stack.
    */
    void unref() const {
        assert(this->getRefCnt() > 0);
        // A release here acts in place of all releases we "should" have been doing in ref().
        if (1 == fRefCnt.fetch_add(-1, std::memory_order_acq_rel)) {
            // Like unique(), the acquire is only needed on success, to make sure
            // code in internal_dispose() doesn't happen before the decrement.
            this->internal_dispose();
        }
    }

private:

#ifdef VLD_DEBUG
    /** Return the reference count. Use only for debugging. */
    int32_t getRefCnt() const {
        return fRefCnt.load(std::memory_order_relaxed);
    }
#endif

    /**
     *  Called when the ref count goes to 0.
     */
    virtual void internal_dispose() const {
    #ifdef VLD_DEBUG
        assert(0 == this->getRefCnt());
        fRefCnt.store(1, std::memory_order_relaxed);
    #endif
        delete this;
    }

    // The following friends are those which override internal_dispose()
    // and conditionally call RefCnt::internal_dispose().
    friend class WeakRefCnt;

    mutable std::atomic<int32_t> fRefCnt;

    RefCntBase(RefCntBase&&) = delete;
    RefCntBase(const RefCntBase&) = delete;
    RefCntBase& operator=(RefCntBase&&) = delete;
    RefCntBase& operator=(const RefCntBase&) = delete;
};

///////////////////////////////////////////////////////////////////////////////

/** Call obj->ref() and return obj. The obj must not be nullptr.
 */
template <typename T> static inline T* Ref(T* obj) {
    assert(obj);
    obj->ref();
    return obj;
}

/** Check if the argument is non-null, and if so, call obj->ref() and return obj.
 */
template <typename T> static inline T* SafeRef(T* obj) {
    if (obj) {
        obj->ref();
    }
    return obj;
}

/** Check if the argument is non-null, and if so, call obj->unref()
 */
template <typename T> static inline void SafeUnref(T* obj) {
    if (obj) {
        obj->unref();
    }
}

///////////////////////////////////////////////////////////////////////////////

// This is a variant of RefCnt that's Not Virtual, so weighs 4 bytes instead of 8 or 16.
// There's only benefit to using this if the deriving class does not otherwise need a vtable.
template <typename Derived>
class NVRefCnt {
public:
    NVRefCnt() : fRefCnt(1) {}
    ~NVRefCnt() {
    #ifdef VLD_DEBUG
        int rc = fRefCnt.load(std::memory_order_relaxed);
        //ASSERTF(rc == 1, "NVRefCnt was %d", rc);
        assert(rc == 1);
    #endif
    }

    // Implementation is pretty much the same as RefCntBase. All required barriers are the same:
    //   - unique() needs acquire when it returns true, and no barrier if it returns false;
    //   - ref() doesn't need any barrier;
    //   - unref() needs a release barrier, and an acquire if it's going to call delete.

    bool unique() const { return 1 == fRefCnt.load(std::memory_order_acquire); }
    void ref() const { (void)fRefCnt.fetch_add(+1, std::memory_order_relaxed); }
    void  unref() const {
        if (1 == fRefCnt.fetch_add(-1, std::memory_order_acq_rel)) {
            // restore the 1 for our destructor's assert
            DEBUGCODE(fRefCnt.store(1, std::memory_order_relaxed));
            delete (const Derived*)this;
        }
    }
    void  deref() const { this->unref(); }

    // This must be used with caution. It is only valid to call this when 'threadIsolatedTestCnt'
    // refs are known to be isolated to the current thread. That is, it is known that there are at
    // least 'threadIsolatedTestCnt' refs for which no other thread may make a balancing unref()
    // call. Assuming the contract is followed, if this returns false then no other thread has
    // ownership of this. If it returns true then another thread *may* have ownership.
    bool refCntGreaterThan(int32_t threadIsolatedTestCnt) const {
        int cnt = fRefCnt.load(std::memory_order_acquire);
        // If this fails then the above contract has been violated.
        ASSERT(cnt >= threadIsolatedTestCnt);
        return cnt > threadIsolatedTestCnt;
    }

private:
    mutable std::atomic<int32_t> fRefCnt;

    NVRefCnt(NVRefCnt&&) = delete;
    NVRefCnt(const NVRefCnt&) = delete;
    NVRefCnt& operator=(NVRefCnt&&) = delete;
    NVRefCnt& operator=(const NVRefCnt&) = delete;
};

///////////////////////////////////////////////////////////////////////////////////////////////////

/**
 *  Shared pointer class to wrap classes that support a ref()/unref() interface.
 *
 *  This can be used for classes inheriting from RefCnt, but it also works for other
 *  classes that match the interface, but have different internal choices: e.g. the hosted class
 *  may have its ref/unref be thread-safe, but that is not assumed/imposed by sp.
 *
 *  Declared with the trivial_abi attribute where supported so that sp and types containing it
 *  may be considered as trivially relocatable by the compiler so that destroying-move operations
 *  i.e. move constructor followed by destructor can be optimized to memcpy.
 */
#if defined(__clang__) && defined(__has_cpp_attribute) && __has_cpp_attribute(clang::trivial_abi)
#define VLD_SP_TRIVIAL_ABI [[clang::trivial_abi]]
#else
#define VLD_SP_TRIVIAL_ABI
#endif
template <typename T> class VLD_SP_TRIVIAL_ABI sp {
public:
    using element_type = T;

    constexpr sp() : fPtr(nullptr) {}
    constexpr sp(std::nullptr_t) : fPtr(nullptr) {}

    /**
     *  Shares the underlying object by calling ref(), so that both the argument and the newly
     *  created sp both have a reference to it.
     */
    sp(const sp<T>& that) : fPtr(SafeRef(that.get())) {}
    template <typename U,
              typename = std::enable_if_t< std::is_convertible<U*, T*>::value > >
    sp(const sp<U>& that) : fPtr(SafeRef(that.get())) {
        //static_assert(std::is_convertible<U*, T*>::value,
        //    "Type not convertable!");
    }

    /**
     *  Move the underlying object from the argument to the newly created sp. Afterwards only
     *  the new sp will have a reference to the object, and the argument will point to null.
     *  No call to ref() or unref() will be made.
     */
    sp(sp<T>&& that) : fPtr(that.release()) {}
    template <typename U,
              typename = typename std::enable_if<std::is_convertible<U*, T*>::value>::type>
    sp(sp<U>&& that) : fPtr(that.release()) { 
        //static_assert(std::is_convertible<U*, T*>::value,
        //    "Type not convertable!");
    }

    /**
     *  Adopt the bare pointer into the newly created sp.
     *  No call to ref() or unref() will be made.
     */
    explicit sp(T* obj) : fPtr(obj) {}

    /**
     *  Calls unref() on the underlying object pointer.
     */
    ~sp() {
        SafeUnref(fPtr);
        DEBUGCODE(fPtr = nullptr);
    }

    sp<T>& operator=(std::nullptr_t) { this->reset(); return *this; }

    /**
     *  Shares the underlying object referenced by the argument by calling ref() on it. If this
     *  sp previously had a reference to an object (i.e. not null) it will call unref() on that
     *  object.
     */
    sp<T>& operator=(const sp<T>& that) {
        if (this != &that) {
            this->reset(SafeRef(that.get()));
        }
        return *this;
    }
    template <typename U>
    typename std::enable_if<std::is_convertible<U*, T*>::value, typename sp<T>&>::type
    operator=(const sp<U>& that) {
        static_assert(std::is_convertible<U*, T*>::value,
            "Type not convertable!");
        this->reset(SafeRef(that.get()));
        return *this;
    }

    /**
     *  Move the underlying object from the argument to the sp. If the sp previously held
     *  a reference to another object, unref() will be called on that object. No call to ref()
     *  will be made.
     */
    sp<T>& operator=(sp<T>&& that) {
        this->reset(that.release());
        return *this;
    }
    template <typename U>
    sp<T>& operator=(sp<U>&& that) {
        static_assert(std::is_convertible<U*, T*>::value,
            "Type not convertable!");
        this->reset(that.release());
        return *this;
    }

    template <typename U>
    bool operator==(const sp<U>& that){ return this->fPtr == that.fPtr;}

    T& operator*() const {
        assert(this->get() != nullptr);
        return *this->get();
    }

    explicit operator bool() const { return this->get() != nullptr; }

    T* get() const { return fPtr; }
    T* operator->() const { return fPtr; }

    /**
     *  Adopt the new bare pointer, and call unref() on any previously held object (if not null).
     *  No call to ref() will be made.
     */
    void reset(T* ptr = nullptr) {
        // Calling fPtr->unref() may call this->~() or this->reset(T*).
        // http://wg21.cmeerw.net/lwg/issue998
        // http://wg21.cmeerw.net/lwg/issue2262
        T* oldPtr = fPtr;
        fPtr = ptr;
        SafeUnref(oldPtr);
    }

    /**
     *  Return the bare pointer, and set the internal object pointer to nullptr.
     *  The caller must assume ownership of the object, and manage its reference count directly.
     *  No call to unref() will be made.
     */
    T* release() {
        T* ptr = fPtr;
        fPtr = nullptr;
        return ptr;
    }

    void swap(sp<T>& that) /*noexcept*/ {
        using std::swap;
        swap(fPtr, that.fPtr);
    }

private:
    T*  fPtr;
};

template <typename T> inline void swap(sp<T>& a, sp<T>& b) /*noexcept*/ {
    a.swap(b);
}

template <typename T, typename U> inline bool operator==(const sp<T>& a, const sp<U>& b) {
    return a.get() == b.get();
}
template <typename T> inline bool operator==(const sp<T>& a, std::nullptr_t) /*noexcept*/ {
    return !a;
}
template <typename T> inline bool operator==(std::nullptr_t, const sp<T>& b) /*noexcept*/ {
    return !b;
}

template <typename T, typename U> inline bool operator!=(const sp<T>& a, const sp<U>& b) {
    return a.get() != b.get();
}
template <typename T> inline bool operator!=(const sp<T>& a, std::nullptr_t) /*noexcept*/ {
    return static_cast<bool>(a);
}
template <typename T> inline bool operator!=(std::nullptr_t, const sp<T>& b) /*noexcept*/ {
    return static_cast<bool>(b);
}

template <typename C, typename CT, typename T>
auto operator<<(std::basic_ostream<C, CT>& os, const sp<T>& sp) -> decltype(os << sp.get()) {
    return os << sp.get();
}

template <typename T, typename... Args>
sp<T> make_sp(Args&&... args) {
    return sp<T>(new T(std::forward<Args>(args)...));
}

/*
 *  Returns a sp wrapping the provided ptr AND calls ref on it (if not null).
 *
 *  This is different than the semantics of the constructor for sp, which just wraps the ptr,
 *  effectively "adopting" it.
 */
template <typename T> sp<T> ref_sp(T* obj) {
    return sp<T>(SafeRef(obj));
}

template <typename T> sp<T> ref_sp(const T* obj) {
    return sp<T>(const_cast<T*>(SafeRef(obj)));
}

}

namespace std {
  template <typename T> struct hash<Veldrid::sp<T>>
  {
    size_t operator()(const Veldrid::sp<T> & x) const
    {
      /* your code here, e.g. "return hash<int>()(x.value);" */
      return hash<void*>()(x.get());
    }
  };
}
