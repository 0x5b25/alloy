#pragma once

#include "d3d12.h"

namespace alloy::dxc
{
    
    class PipelineStateStream {
        static constexpr size_t ALIGN_BYTES = sizeof(void*);

        static constexpr auto ALIGN(size_t val) {
            return (val + ALIGN_BYTES - 1) & ~(ALIGN_BYTES - 1);
        }

        void* _pBuffer = nullptr;
        size_t _size = 0, _capacity = 0;

        void _GrowBuffer(size_t reqCapBytes) {
            auto newCap = _capacity;
            if(newCap == 0) {
                auto alignedReqCapBytes = ALIGN(reqCapBytes);
                newCap = alignedReqCapBytes;
            } else {
                while(newCap < reqCapBytes) {
                    newCap <<= 1; //Grow by factor of 2
                }
            }

            void* pNewBuffer = malloc(newCap);
            if(_size) {
                memcpy(pNewBuffer, _pBuffer, _size);
                free(_pBuffer);
            }
            _capacity = newCap;
            _pBuffer = pNewBuffer;
        }

        void* _AllocBack(size_t reqSizeBytes) {
            auto alignedStart = ALIGN(_size);
            auto alignedReqCap = alignedStart + reqSizeBytes;
            if(alignedReqCap > _capacity) {
                _GrowBuffer(alignedReqCap);
            }

            _size = alignedReqCap;

            return (void*)((uintptr_t)_pBuffer + alignedStart);
        }

    public:
        PipelineStateStream() = default;
        ~PipelineStateStream() {
            free(_pBuffer);
        }

        PipelineStateStream(const PipelineStateStream&) = delete;
        PipelineStateStream& operator=(const PipelineStateStream&) = delete;

        
        PipelineStateStream(PipelineStateStream&& other)
            : _pBuffer(other._pBuffer)
            , _size(other._size)
            , _capacity(other._capacity)
        { 
            other._pBuffer = nullptr;
            other._size = 0;
            other._capacity = 0;
        }
        PipelineStateStream& operator=(PipelineStateStream&& other) {
            if(_size) {
                free(_pBuffer);
            }

            _pBuffer = other._pBuffer;
            _size = other._size;
            _capacity = other._capacity;
            other._pBuffer = nullptr;
            other._size = 0;
            other._capacity = 0;
        }

        template<typename InnerStructType,  D3D12_PIPELINE_STATE_SUBOBJECT_TYPE Tag>
        InnerStructType& Append() {
            struct Block {
                D3D12_PIPELINE_STATE_SUBOBJECT_TYPE tag;
                InnerStructType data;
            };

            auto blockSize = ALIGN(sizeof(InnerStructType) + sizeof(Tag));
            auto pBlock = (Block*)_AllocBack(blockSize);
            //Run a default ctor
            new(pBlock)Block{};
            pBlock->tag = Tag;
            return pBlock->data;
        }

        explicit operator D3D12_PIPELINE_STATE_STREAM_DESC() const {
            return {
                .SizeInBytes = _size,
                .pPipelineStateSubobjectStream = _pBuffer
            };
        }

    };

} // namespace alloy::dxc

