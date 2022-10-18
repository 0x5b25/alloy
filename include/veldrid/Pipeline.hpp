#pragma once

#include "veldrid/DeviceResource.hpp"
#include "veldrid/FixedFunctions.hpp"
#include "veldrid/Shader.hpp"


#include <cstdint>
#include <string>
#include <vector>

namespace Veldrid
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

    struct GraphicsPipelineDescription{

        // A description of the blend state, which controls how color values are blended into each color target.
        BlendStateDescription blendState;
        
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

            struct VertexLayout{

                struct Element{
                    // The name of the element.
                    std::string name;
                    /// The semantic type of the element.
                    /// NOTE: When using Veldrid.SPIRV, all vertex elements will use
                    /// <see cref="VertexElementSemantic.TextureCoordinate"/>.
                    enum class Semantic : std::uint8_t{
                        Position, Normal, 
                        TextureCoordinate, Color,
                    } semantic;
                    
                    // The format of the element.
                    ShaderDataType format;
                    
                    // The offset in bytes from the beginning of the vertex.
                    std::uint32_t offset;
                };
                
                // The number of bytes in between successive elements in the <see cref="DeviceBuffer"/>.
                std::uint32_t stride;
                
                // An array of <see cref="VertexElementDescription"/> objects, each describing a single element of vertex data.
                std::vector<Element*> elements;
                
                /// A value controlling how often data for instances is advanced for this layout. For per-vertex elements, this value
                /// should be 0.
                /// For example, an InstanceStepRate of 3 indicates that 3 instances will be drawn with the same value for this layout. The
                /// next 3 instances will be drawn with the next value, and so on.
                std::uint32_t instanceStepRate;
            };

            std::vector<VertexLayout*> vertexLayouts;
            std::vector<sp<Shader>> shaders;
            std::vector<SpecializationConstant*> specializations;

        } shaderSet;
        
        // An array of <see cref="ResourceLayout"/>, which controls the layout of shader resources in the <see cref="Pipeline"/>.
        std::vector<sp<ResourceLayout>> resourceLayouts;
        
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
        sp<Shader> computeShader;
        
        // An array of <see cref="ResourceLayout"/>, which controls the layout of shader resoruces in the <see cref="Pipeline"/>.
        std::vector<sp<ResourceLayout>> resourceLayouts;
        
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
    

    class Pipeline : public DeviceResource{

    protected:
        Pipeline(sp<GraphicsDevice>&& dev) : DeviceResource(std::move(dev)){}

    public:
        
    public:

        virtual bool IsComputePipeline() const = 0;        

    };
} // namespace Veldrid
