#pragma once

#include "common/RefCnt.hpp"

#include <vector>

namespace alloy
{
    class IResourceLayout;

    struct IndirectCommandArgDraw {

    };

    struct IndirectCommandArgDrawIndexed {

    };

    struct IndirectCommandArgDispatch {

    };

    struct IndirectCommandArgBindIndexBuffer {

    };

    struct IndirectCommandArgBindVertexBuffer {

    };

    class IIndirectCommandLayout : public common::RefCntBase {

    public:

        enum class OpType {
            Draw,
            DrawIndexed,
            Dispatch,
            BindVertexBuffer,
            BindIndexBuffer,
            SetPushConstant,
            BindCBV,
            BindSRV,
            BindUAV,
            DispatchRays, //Not implemented
            DispatchMesh, //Not implemented
        };

        struct Operation {
            OpType type;

            union {
                struct {
                    std::uint32_t slot;
                } bindVertexBuffer;

                struct {
                    std::uint32_t pushConstantIndex;
                    std::uint32_t destOffsetIn32BitValues;
                    std::uint32_t num32BitValuesToSet;
                } setPushConstant;

                struct {
                    std::uint32_t layoutIndex;
                } bindCBV, BindSRV, BindUAV;
            };
        };

        struct Description {

            uint32_t argumentStrideInBytes; // Stride in bytes to fetch arguments for next command 
            std::vector<Operation> operations;

            common::sp<IResourceLayout> layout; //optional if no operation will alter resource sets
        };


    public:
        virtual ~IIndirectCommandLayout() {}

    };


} // namespace alloy

