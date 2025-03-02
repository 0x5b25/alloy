#pragma once

#include "alloy/Types.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace alloy
{
    // The <see cref="ComparisonKind"/> used when considering new depth values.
    enum class ComparisonKind{
        Never, Always,
        Less, LessEqual, Equal, GreaterEqual, Greater,
        NotEqual
    };

    struct AttachmentStateDescription{

        enum class BlendFactor{
            /// <summary>
            /// Each component is multiplied by 0.
            /// </summary>
            Zero,
            /// <summary>
            /// Each component is multiplied by 1.
            /// </summary>
            One,
            /// <summary>
            /// Each component is multiplied by the source alpha component.
            /// </summary>
            SourceAlpha,
            /// <summary>
            /// Each component is multiplied by (1 - source alpha).
            /// </summary>
            InverseSourceAlpha,
            /// <summary>
            /// Each component is multiplied by the destination alpha component.
            /// </summary>
            DestinationAlpha,
            /// <summary>
            /// Each component is multiplied by (1 - destination alpha).
            /// </summary>
            InverseDestinationAlpha,
            /// <summary>
            /// Each component is multiplied by the matching component of the source color.
            /// </summary>
            SourceColor,
            /// <summary>
            /// Each component is multiplied by (1 - the matching component of the source color).
            /// </summary>
            InverseSourceColor,
            /// <summary>
            /// Each component is multiplied by the matching component of the destination color.
            /// </summary>
            DestinationColor,
            /// <summary>
            /// Each component is multiplied by (1 - the matching component of the destination color).
            /// </summary>
            InverseDestinationColor,
            /// <summary>
            /// Each component is multiplied by the matching component in constant factor specified in <see cref="BlendStateDescription.BlendFactor"/>.
            /// </summary>
            BlendFactor,
            /// <summary>
            /// Each component is multiplied by (1 - the matching component in constant factor specified in <see cref="BlendStateDescription.BlendFactor"/>).
            /// </summary>
            InverseBlendFactor,
        };

        enum class BlendFunction{
            /// <summary>
            /// Source and destination are added.
            /// </summary>
            Add,
            /// <summary>
            /// Destination is subtracted from source.
            /// </summary>
            Subtract,
            /// <summary>
            /// Source is subtracted from destination.
            /// </summary>
            ReverseSubtract,
            /// <summary>
            /// The minimum of source and destination is selected.
            /// </summary>
            Minimum,
            /// <summary>
            /// The maximum of source and destination is selected.
            /// </summary>
            Maximum,
        };

        union ColorWriteMask{
            struct{
                bool r : 1;
                bool g : 1;
                bool b : 1;
                bool a : 1;
            };
            std::uint8_t value;

            static constexpr ColorWriteMask All(){
                ColorWriteMask mask = {1,1,1,1};
                return mask;
            }
        };

        struct ColorAttachment{

            PixelFormat format;

            // Controls whether blending is enabled for the color attachment.
            bool blendEnabled;

            /// Controls which components of the color will be written to the framebuffer.
            ColorWriteMask colorWriteMask;
            
            // Controls the source color's influence on the blend result.
            BlendFactor sourceColorFactor;
            
            // Controls the destination color's influence on the blend result.
            BlendFactor destinationColorFactor;
            
            // Controls the function used to combine the source and destination color factors.
            BlendFunction colorFunction;
            
            // Controls the source alpha's influence on the blend result.
            BlendFactor sourceAlphaFactor;
            
            // Controls the destination alpha's influence on the blend result.
            BlendFactor destinationAlphaFactor;
            
            // Controls the function used to combine the source and destination alpha factors.
            BlendFunction alphaFunction;

            /// <summary>
            /// Describes a blend attachment state in which the source completely overrides the destination.
            /// Settings:
            ///     BlendEnabled = true
            ///     ColorWriteMask = null
            ///     SourceColorFactor = BlendFactor.One
            ///     DestinationColorFactor = BlendFactor.Zero
            ///     ColorFunction = BlendFunction.Add
            ///     SourceAlphaFactor = BlendFactor.One
            ///     DestinationAlphaFactor = BlendFactor.Zero
            ///     AlphaFunction = BlendFunction.Add
            /// </summary>
            static constexpr ColorAttachment MakeOverrideBlend(){
                ColorAttachment res{};
                res.blendEnabled = true;
                res.sourceColorFactor = BlendFactor::One;
                res.destinationColorFactor = BlendFactor::Zero;
                res.colorFunction = BlendFunction::Add;
                res.sourceAlphaFactor = BlendFactor::One;
                res.destinationAlphaFactor = BlendFactor::Zero;
                res.alphaFunction = BlendFunction::Add;
                res.colorWriteMask.value = 0xf;
                return res;
            };

            /// <summary>
            /// Describes a blend attachment state in which the source and destination are blended in an inverse relationship.
            /// Settings:
            ///     BlendEnabled = true
            ///     ColorWriteMask = null
            ///     SourceColorFactor = BlendFactor.SourceAlpha
            ///     DestinationColorFactor = BlendFactor.InverseSourceAlpha
            ///     ColorFunction = BlendFunction.Add
            ///     SourceAlphaFactor = BlendFactor.SourceAlpha
            ///     DestinationAlphaFactor = BlendFactor.InverseSourceAlpha
            ///     AlphaFunction = BlendFunction.Add
            /// </summary>
            static constexpr ColorAttachment MakeAlphaBlend() {
                ColorAttachment res{};
                res.blendEnabled = true;
                res.sourceColorFactor = BlendFactor::SourceAlpha;
                res.destinationColorFactor = BlendFactor::InverseSourceAlpha;
                res.colorFunction = BlendFunction::Add;
                res.sourceAlphaFactor = BlendFactor::SourceAlpha;
                res.alphaFunction = BlendFunction::Add;
                res.destinationAlphaFactor = BlendFactor::InverseSourceAlpha;
                res.colorWriteMask.value = 0xf;
                return res;
            };

            /// <summary>
            /// Describes a blend attachment state in which the source is added to the destination based on its alpha channel.
            /// Settings:
            ///     BlendEnabled = true
            ///     ColorWriteMask = null
            ///     SourceColorFactor = BlendFactor.SourceAlpha
            ///     DestinationColorFactor = BlendFactor.One
            ///     ColorFunction = BlendFunction.Add
            ///     SourceAlphaFactor = BlendFactor.SourceAlpha
            ///     DestinationAlphaFactor = BlendFactor.One
            ///     AlphaFunction = BlendFunction.Add
            /// </summary>
            static constexpr ColorAttachment MakeAdditiveBlend() {
                ColorAttachment res{};
                res.blendEnabled = true;
                res.sourceColorFactor = BlendFactor::SourceAlpha;
                res.destinationColorFactor = BlendFactor::One;
                res.colorFunction = BlendFunction::Add;
                res.sourceAlphaFactor = BlendFactor::SourceAlpha;
                res.alphaFunction = BlendFunction::Add;
                res.destinationAlphaFactor = BlendFactor::One;
                res.colorWriteMask.value = 0xf;
                return res;
            };

            /// <summary>
            /// Describes a blend attachment state in which blending is disabled.
            /// Settings:
            ///     BlendEnabled = false
            ///     ColorWriteMask = null
            ///     SourceColorFactor = BlendFactor.One
            ///     DestinationColorFactor = BlendFactor.Zero
            ///     ColorFunction = BlendFunction.Add
            ///     SourceAlphaFactor = BlendFactor.One
            ///     DestinationAlphaFactor = BlendFactor.Zero
            ///     AlphaFunction = BlendFunction.Add
            /// </summary>
            static constexpr ColorAttachment MakeDisabled() {
                ColorAttachment res{};
                res.blendEnabled = false;
                res.sourceColorFactor = BlendFactor::One;
                res.destinationColorFactor = BlendFactor::Zero;
                res.colorFunction = BlendFunction::Add;
                res.sourceAlphaFactor = BlendFactor::One;
                res.alphaFunction = BlendFunction::Add;
                res.destinationAlphaFactor = BlendFactor::Zero;
                res.colorWriteMask.value = 0xf;
                return res;
            };
            
        };

        // A constant blend color used in <see cref="BlendFactor.BlendFactor"/> and <see cref="BlendFactor.InverseBlendFactor"/>,
        // or otherwise ignored.
        Color4f blendConstant;

        SampleCount sampleCount;

        std::vector<ColorAttachment> colorAttachments;

        struct DepthStencilAttachment{
            PixelFormat depthStencilFormat;
        };

        std::optional<DepthStencilAttachment> depthStencilAttachment;
        
        // Enables alpha-to-coverage, which causes a fragment's alpha value to be used when determining multi-sample coverage.
        bool alphaToCoverageEnabled;
    };

    ///#TODO: unify DepthStencilStateDescription and AttachmentState
    struct DepthStencilStateDescription{
        
        // Controls whether depth testing is enabled.
        bool depthTestEnabled;
        
        // Controls whether new depth values are written to the depth buffer.
        bool depthWriteEnabled;
       
        ComparisonKind depthComparison;

        // Controls whether the stencil test is enabled.
        bool stencilTestEnabled;
        
        struct StencilBehavior{
            enum class Operation{
                Keep, Zero, Replace, Invert,
                IncrementAndClamp, IncrementAndWrap,
                DecrementAndClamp, DecrementAndWrap
            } fail, pass, depthFail;

            ComparisonKind comparison;
        };
        // Controls how stencil tests are handled for pixels whose surface faces towards the camera.
        StencilBehavior stencilFront;
        
        // Controls how stencil tests are handled for pixels whose surface faces away from the camera.
        StencilBehavior stencilBack;
        
        //TODO: [VK] Convert to dynamic state and exclude from PSO
        // Controls the portion of the stencil buffer used for reading.
        std::uint8_t stencilReadMask;
        
        //TODO: [VK] Convert to dynamic state and exclude from PSO
        // Controls the portion of the stencil buffer used for writing.
        std::uint8_t stencilWriteMask;
        
        //TODO: [VK] Convert to dynamic state and exclude from PSO
        // The reference value to use when doing a stencil test.
        std::uint32_t stencilReference;
    };

    struct RasterizerStateDescription{
        
        // Controls which face will be culled.
        enum class FaceCullMode{
            Back,Front,None,
        } cullMode;
        
        // Controls how the rasterizer fills polygons.
        enum class PolygonFillMode{
            Solid, Wireframe
        } fillMode;
       
        // Controls the winding order used to determine the front face of primitives.
        enum class FrontFace{
            Clockwise, CounterClockwise
        } frontFace;
        
        // Controls whether depth clipping is enabled.
        bool depthClipEnabled;
        
        // Controls whether the scissor test is enabled.
        bool scissorTestEnabled;
    };
    
    // The <see cref="PrimitiveTopology"/> to use, which controls how a series of input vertices is interpreted by the
    enum class PrimitiveTopology : std::uint8_t
    {
        // A list of isolated, 3-element triangles.
        TriangleList,
        
        // A series of connected triangles.
        TriangleStrip,
        
        // A series of isolated, 2-element line segments.
        LineList,
        
        // A series of connected line segments.
        LineStrip,
        
        // A series of isolated points.
        PointList,
    };    
    
    // Specifies which model the rendering backend should use for binding resources.
    enum class ResourceBindingModel{
        Default = 0, Improved = 1,
    };

    

} // namespace alloy


