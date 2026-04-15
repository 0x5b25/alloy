#pragma once

#include <vector>
#include <vulkan/vulkan_core.h>

namespace alloy::vk {

    class StructStream {
        //For allocation and chaining
        struct VkStructStub {
            VkStructureType            sType;
            void*                      pNext;
        };

        std::vector<VkStructStub*> _structs;

    public:
        StructStream() = default;
        ~StructStream() {
            for(auto* p : _structs) {
                free(p);
            }
        }

        StructStream(const StructStream&) = delete;
        StructStream& operator=(const StructStream&) = delete;


        template<typename T, VkStructureType tag>
        T& Append() {

            static_assert( offsetof(T, sType) == offsetof(VkStructStub, sType) &&
                           offsetof(T, pNext) == offsetof(VkStructStub, pNext) );

            auto pStruct = (T*)malloc(sizeof(T));
            *pStruct = T{ .sType = tag };

            if(!_structs.empty()) {
                _structs.back()->pNext = pStruct;
            }

            _structs.push_back((VkStructStub*)pStruct);
            return *pStruct;
        }

        template<typename T>
        T* Front() {
            if(_structs.empty()) return nullptr;
            return (T*)_structs.front();
        }

    };

}
