#pragma once

#include <atomic>       // std::atomic, std::memory_order_*
#include <mutex>
#include <cstddef>      // std::nullptr_t
#include <iosfwd>       // std::basic_ostream
#include <memory>       // TODO: unused
#include <type_traits>  // std::enable_if, std::is_convertible
#include <utility>      // std::forward, std::swap
#include <cassert>

namespace alloy{
    template<typename T>
    class Singleton{
        static std::atomic<T*> _pinstance;
        static std::mutex _m;

    public:
        static T* Get(){
            if (_pinstance == nullptr) {
                std::lock_guard<std::mutex> lock{ _m };
                if (_pinstance == nullptr) {
                    _pinstance = new T();
                }
            }
            return _pinstance.load();
        }

    };

    template<typename T> std::atomic<T*> Singleton<T>::_pinstance{ nullptr };
    template<typename T> std::mutex Singleton<T>::_m{};

}