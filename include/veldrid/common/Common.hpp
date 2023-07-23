#pragma once

#include "Macros.h"
#include "RefCnt.hpp"

namespace Veldrid
{
    template<typename T, typename U>
    T* PtrCast(U* ptr){
    #if 0 //VLD_DEBUG
        T* res = dynamic_cast<T*>(ptr);
        //Actually is the target type
        assert(res != nullptr);
        //And can be done using a reinterpret_cast
        assert((void*)res == (void*)ptr);
        return res;
    #else
        return static_cast<T*>(ptr);
    #endif
    }

    template<typename T, typename U>
    sp<T> SPCast(const sp<U>& ptr) {
        return RefRawPtr<T>(PtrCast<T>(ptr.get()));
    }

    template<typename T>
    sp<T> RefRawPtr(T* ptr){
        ptr->ref();
        return sp{ptr};        
    }

} // namespace Veldrid



