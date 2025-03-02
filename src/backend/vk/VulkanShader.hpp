#pragma once

#include <volk.h>

#include "alloy/Shader.hpp"
#include <dxil_spirv_c.h>

#include <string>
#include <vector>
#include <unordered_map>
#include <span>
#include <functional>

namespace alloy::vk {
    enum ShaderConverterResult {
        Success,
        VKD3D_ERROR_INVALID_SHADER,
        VKD3D_ERROR_INVALID_ARGUMENT,
        VKD3D_ERROR_NOT_IMPLEMENTED
    };

    struct ShaderMetadata
    {
        uint64_t _dummy;//vkd3d_shader_hash_t hash;

        unsigned int cs_workgroup_size[3]; /* Only contains valid data if uses_subgroup_size is true. */
        unsigned int patch_vertex_count; /* Relevant for HS. May be 0, in which case the patch vertex count is not known. */
        uint8_t cs_wave_size_min; /* If non-zero, minimum or required subgroup size. */
        uint8_t cs_wave_size_max; /* If non-zero, maximum subgroup size. */
        uint8_t cs_wave_size_preferred; /* If non-zero, preferred subgroup size. */
        uint8_t reserved;
        uint32_t flags; /* vkd3d_shader_meta_flags */
    };
    static_assert(sizeof(ShaderMetadata) == 32);

    struct SPIRVBlob
    {
        std::vector<uint8_t> code;
        ShaderMetadata meta;
    };

    
    struct ConverterCompilerArgs
    {
        //enum vkd3d_shader_target target;

        VkShaderStageFlagBits shaderStage;

        std::string entryPoint;

        //std::vector< vkd3d_shader_target_extension > target_extensions;

        //std::vector<vkd3d_shader_parameter> parameters;
        uint32_t root_constant_words;
        bool dual_source_blending;
        const unsigned int *output_swizzles;
        unsigned int output_swizzle_count;

        uint32_t min_subgroup_size;
        uint32_t max_subgroup_size;
        bool promote_wave_size_heuristics;

        //const struct vkd3d_shader_quirk_info *quirks;
        /* Only non-zero when enabled by vkd3d_config */
        VkDriverId driver_id;
        uint32_t driver_version;
    };

    class SPVRemapper {

    public:
        //using VertexInputRemapFn = std::function<bool(const dxil_spv_d3d_vertex_input&,
        //                                                    dxil_spv_vulkan_vertex_input&)>;
        //using SRVRemapFn = std::function<bool(const dxil_spv_d3d_binding&,
        //                                            dxil_spv_srv_vulkan_binding&)>;
        //using SamplerRemapFn = std::function<bool( const dxil_spv_d3d_binding& binding,
        //                                                 dxil_spv_vulkan_binding& vk_binding)>;
        //using UAVRemapFn = std::function<bool( const dxil_spv_uav_d3d_binding& binding,
        //                                             dxil_spv_uav_vulkan_binding& vk_binding )>;
        //using CBVRemapFn = std::function< bool( const dxil_spv_d3d_binding& binding,
        //                                              dxil_spv_cbv_vulkan_binding& vk_binding)>;
        //
        //struct RemapFn {
        //    VertexInputRemapFn vertexRemapFn;
        //    SRVRemapFn         srvRemapFn;
        //    SamplerRemapFn     samplerRemapFn;
        //    UAVRemapFn         uavRemapFn;
        //    CBVRemapFn         cbvRemapFn;
        //};

    protected:

        struct ShaderStageIOInfo {
            std::string semanticName;
            unsigned int semanticIndex;
            unsigned int vk_location;
            unsigned int vk_component;
            unsigned int vk_flags;
        };

        IShader::Stage currentStage;

        std::unordered_map<std::string, ShaderStageIOInfo> shaderStageIoMap;

        //RemapFn _remapFn;

    public:


        SPVRemapper() { }

        virtual ~SPVRemapper() { }

        void SetStage(IShader::Stage stage) {currentStage = stage;}

        virtual bool RemapSRV( const dxil_spv_d3d_binding& binding,
                              dxil_spv_srv_vulkan_binding& vk_binding
        );

        virtual bool RemapSampler( const dxil_spv_d3d_binding& binding,
                                  dxil_spv_vulkan_binding& vk_binding
        );

        virtual bool RemapUAV ( const dxil_spv_uav_d3d_binding& binding,
                               dxil_spv_uav_vulkan_binding& vk_binding
        ) ;

        virtual bool RemapCBV( const dxil_spv_d3d_binding& binding,
                              dxil_spv_cbv_vulkan_binding& vk_binding
        ) ;

        
        virtual bool RemapVertexInput( const dxil_spv_d3d_vertex_input& d3d_input,
                                      dxil_spv_vulkan_vertex_input& vk_input
        );

        
        virtual bool RemapShaderStageInput( const dxil_spv_d3d_shader_stage_io& d3d_input,
                                        dxil_spv_vulkan_shader_stage_io& vulkan_variable
        );

        virtual bool CaptureShaderStageOutput( const dxil_spv_d3d_shader_stage_io& d3d_input,
                                        dxil_spv_vulkan_shader_stage_io& vulkan_variable
        );

    };

    ShaderConverterResult DXIL2SPV(const std::span<uint8_t>& dxil,
                                   const ConverterCompilerArgs& compiler_args,
                                   SPVRemapper& remapper,
                                   SPIRVBlob& spirv);

    
    class VulkanDevice;

    class VulkanShader : public IShader{

        common::sp<VulkanDevice> _dev;
        
    private:
        //VkShaderModule _shaderModule;
        //std::string _name;
        
        std::vector<std::uint8_t> _il;
        //VkShaderModule ShaderModule => _shaderModule;

        VulkanShader(
            const common::sp<VulkanDevice>& dev,
            const Description& desc,
            const std::span<std::uint8_t>& il
        ) 
            : IShader(desc)
            , _dev(dev)
            , _il(il.begin(), il.end())
        {}
       

    public:
        //const VkShaderModule& GetHandle() const { return _shaderModule; }

        virtual const std::span<uint8_t> GetByteCode() override { return _il; }

        ~VulkanShader();

        static common::sp<IShader> Make(
            const common::sp<VulkanDevice>& dev,
            const IShader::Description& desc,
            const std::span<std::uint8_t>& il
        );

    };

} // namespace alloy

