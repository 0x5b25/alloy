#pragma once

#include "alloy/Helpers.hpp"
#include "alloy/BindableResource.hpp"
#include "alloy/FixedFunctions.hpp"
#include "alloy/Shader.hpp"
#include "alloy/FrameBuffer.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace alloy
{

    struct SpecializationConstant{
        // The constant variable ID, as defined in the <see cref="Shader"/>.
        std::uint32_t id;
        
        // The type of data stored in this instance. Must be a scalar numeric type.
        ShaderConstantType type;

        // An 8-byte block storing the contents of the specialization value. This is treated as an untyped buffer and is
        // interepreted according to <see cref="Type"/>.
        std::uint64_t data;
    };

    struct VertexInputSemantic {
        enum class Name {
            Binormal,
            BlendIndices,
            BlendWeight,
            Color,
            Normal,
            Position,
            PointSize,
            Tangent,
            TextureCoordinate
        } name;

        uint32_t slot;

        bool operator==(const VertexInputSemantic& other) const
        {
            return name == other.name && slot == other.slot;
        }
    };

    constexpr const char* SemanticToStr(VertexInputSemantic::Name semantic)  {

        switch(semantic){
            case VertexInputSemantic::Name::Binormal:          return "BINORMAL";
            case VertexInputSemantic::Name::BlendIndices:      return "BLENDINDICES";
            case VertexInputSemantic::Name::BlendWeight:       return "BLENDWEIGHT";
            case VertexInputSemantic::Name::Color:             return "COLOR";
            case VertexInputSemantic::Name::Normal:            return "NORMAL";
            case VertexInputSemantic::Name::Position:          return "POSITION";
            case VertexInputSemantic::Name::PointSize:         return "PSIZE";
            case VertexInputSemantic::Name::Tangent:           return "TANGENT";
            case VertexInputSemantic::Name::TextureCoordinate: return "TEXCOORD";
                
            default: return "";
        };
    }

    struct VertexLayout{

        struct Element{
            // The name of the element.
            std::string name;
            
            /// The semantic type of the element.
            ///#TODO: Use real semantic name and index in shader converter/dxcpipeline
            VertexInputSemantic semantic;
            
            // The format of the element.
            ShaderDataType format;
            
            // The offset in bytes from the beginning of the vertex.
            // If any vertex element has an explicit offset, then all elements must have an explicit offset.
            std::uint32_t offset;
        };
    
        // The number of bytes in between successive elements in the <see cref="DeviceBuffer"/>.
        std::uint32_t stride;
    
        // An array of <see cref="VertexElementDescription"/> objects, each describing a single element of vertex data.
        std::vector<Element> elements;
    
        /// A value controlling how often data for instances is advanced for this layout. For per-vertex elements, this value
        /// should be 0.
        /// For example, an InstanceStepRate of 3 indicates that 3 instances will be drawn with the same value for this layout. The
        /// next 3 instances will be drawn with the next value, and so on.
        std::uint32_t instanceStepRate;

        void SetElements(std::initializer_list<Element> elements){
            
            this->elements = elements;
            unsigned computedStride = 0;
            for (int i = 0; i < elements.size(); i++)
            {
                auto& thisElem = this->elements[i];
                unsigned elementSize = alloy::FormatHelpers::GetSizeInBytes(thisElem.format);
                if (thisElem.offset != 0){
                    computedStride = thisElem.offset + elementSize;
                } else {
                    computedStride += elementSize;
                }
            }
            
            stride = computedStride;
            instanceStepRate = 0;
        }
    };

    struct GraphicsPipelineDescription{

        // A description of the blend state, which controls how color values are blended into each color target.
        AttachmentStateDescription attachmentState;
        
        // A description of the depth stencil state, which controls depth tests, writing, and comparisons.
        DepthStencilStateDescription depthStencilState;
        
        // A description of the rasterizer state, which controls culling, clipping, scissor, and polygon-fill behavior.
        RasterizerStateDescription rasterizerState;
        
        // The <see cref="PrimitiveTopology"/> to use, which controls how a series of input vertices is interpreted by the
        PrimitiveTopology primitiveTopology;
        /// <summary>
        /// A description of the shader set to be used.
        /// </summary>
        struct ShaderSet{

            std::vector<VertexLayout> vertexLayouts;
            //std::vector<sp<Shader>> shaders;
            common::sp<IShader> vertexShader;
            common::sp<IShader> fragmentShader;
            std::vector<SpecializationConstant> specializations;

        } shaderSet;
        
        // An array of <see cref="ResourceLayout"/>, which controls the layout of shader resources in the <see cref="Pipeline"/>.
        common::sp<IResourceLayout> resourceLayout;
        
        // A description of the output attachments used by the <see cref="Pipeline"/>.
        OutputDescription outputs;
        /// <summary>
        /// Specifies which model the rendering backend should use for binding resources.
        /// If <code>null</code>, the pipeline will use the value specified in <see cref="GraphicsDeviceOptions"/>.
        /// </summary>
        ResourceBindingModel* resourceBindingModel;
    };


    struct ComputePipelineDescription
    {
        // The compute <see cref="Shader"/> to be used in the Pipeline. This must be a Shader with
        // <see cref="ShaderStages.Compute"/>.
        common::sp<IShader> computeShader;
        
        // An array of <see cref="ResourceLayout"/>, which controls the layout of shader resoruces in the <see cref="Pipeline"/>.
        common::sp<IResourceLayout> resourceLayout;
        
        // The X dimension of the thread group size.
        std::uint32_t threadGroupSizeX;
        
        // The Y dimension of the thread group size.
        std::uint32_t threadGroupSizeY;

        // The Z dimension of the thread group size.
        std::uint32_t threadGroupSizeZ;
        
        // An array of <see cref="SpecializationConstant"/> used to override specialization constants in the created
        // <see cref="Pipeline"/>. Each element in this array describes a single ID-value pair, which will be matched with the
        // constants specified in the <see cref="Shader"/>.
        
        std::vector<SpecializationConstant*> specializations;

    };
    

    class IGfxPipeline : public common::RefCntBase{

    protected:
        IGfxPipeline() {}

    public:
        
    public:



    };

    class IComputePipeline : public common::RefCntBase{

    protected:
        IComputePipeline() {}

    public:
        
    public:


    };
} // namespace alloy

