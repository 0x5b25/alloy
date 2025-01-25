#pragma once

#include "alloy/common/BitFlags.hpp"
#include "alloy/common/RefCnt.hpp"

#include <cstdint>
#include <string>
#include <vector>
#include <span>
#include <unordered_map>

//#include "common/error.h"
//#include "common/macros.h"

//VKBP_DISABLE_WARNINGS()
//#include <glslang/Public/ShaderLang.h>
//#include <vulkan/vulkan.h>
//#include <spirv_glsl.hpp>
//VKBP_ENABLE_WARNINGS()

//#include "common/vk_common.h"
//#include "core/shader_module.h"
//#include "common/strings.h"

//#include "Device.hpp"

#if defined(VK_USE_PLATFORM_XLIB_KHR)
#	undef None
#endif

namespace alloy
{

    class IShader : public common::RefCntBase
    {

    public:

		enum class Stage {
			// The vertex shader stage.
			Vertex,
			// The geometry shader stage.
			Geometry,
			// The tessellation control (or hull) shader stage.
			TessellationControl,
			// The tessellation evaluation (or domain) shader stage.
			TessellationEvaluation,
			// The fragment (or pixel) shader stage.
			Fragment,
			// The compute shader stage.
			Compute,

			MAX_VALUE
		};

		using Stages = alloy::common::BitFlags<Stage>;

        struct Description{

            Stage stage;
            //std::vector<std::uint8_t> bytes;
            std::string entryPoint;
            bool enableDebug;
        };

    protected:

        Description description;

        IShader(const Description& desc
        ) : 
            description(desc){}

    public: 
        const Description& GetDesc() const {return description;}
        
        virtual const std::span<uint8_t> GetByteCode() = 0;
    };

  


    /// Types of shader resources
    enum class ShaderResourceType
    {
    	Input,
    	InputAttachment,
    	Output,
    	Image,
    	ImageSampler,
    	ImageStorage,
    	Sampler,
    	BufferUniform,
    	BufferStorage,
    	PushConstant,
    	SpecializationConstant,
    	All
    };

    /// This determines the type and method of how descriptor set should be created and bound
    enum class ShaderResourceMode
    {
    	Static,
    	Dynamic,
    	UpdateAfterBind
    };

    /// A bitmask of qualifiers applied to a resource
    struct ShaderResourceQualifiers
    {
    	enum : uint32_t
    	{
    		None        = 0,
    		NonReadable = 1,
    		NonWritable = 2,
    	};
    };
#if 0
    /// Store shader resource data.
    /// Used by the shader module.
    struct ShaderResource
    {
    	//VkShaderStageFlags stages;

    	ShaderResourceType type;

    	ShaderResourceMode mode;

    	uint32_t set;

    	uint32_t binding;

    	uint32_t location;

    	uint32_t input_attachment_index;

    	uint32_t vec_size;

    	uint32_t columns;

    	uint32_t array_size;

    	uint32_t offset;

    	uint32_t size;

    	uint32_t constant_id;

    	uint32_t qualifiers;

    	std::string name;
    };

    /**
     * @brief Adds support for C style preprocessor macros to glsl shaders
     *        enabling you to define or undefine certain symbols
     */
    class ShaderVariant
    {
      public:
    	ShaderVariant() = default;

    	ShaderVariant(std::string &&preamble, std::vector<std::string> &&processes);

    	size_t get_id() const;

    	/**
    	 * @brief Add definitions to shader variant
    	 * @param definitions Vector of definitions to add to the variant
    	 */
    	void add_definitions(const std::vector<std::string> &definitions);

    	/**
    	 * @brief Adds a define macro to the shader
    	 * @param def String which should go to the right of a define directive
    	 */
    	void add_define(const std::string &def);

    	/**
    	 * @brief Adds an undef macro to the shader
    	 * @param undef String which should go to the right of an undef directive
    	 */
    	void add_undefine(const std::string &undef);

    	/**
    	 * @brief Specifies the size of a named runtime array for automatic reflection. If already specified, overrides the size.
    	 * @param runtime_array_name String under which the runtime array is named in the shader
    	 * @param size Integer specifying the wanted size of the runtime array (in number of elements, not size in bytes), used for automatic allocation of buffers.
    	 * See get_declared_struct_size_runtime_array() in spirv_cross.h
    	 */
    	void add_runtime_array_size(const std::string &runtime_array_name, size_t size);

    	void set_runtime_array_sizes(const std::unordered_map<std::string, size_t> &sizes);

    	const std::string &get_preamble() const;

    	const std::vector<std::string> &get_processes() const;

    	const std::unordered_map<std::string, size_t> &get_runtime_array_sizes() const;

    	void clear();

      private:
    	size_t id;

    	std::string preamble;

    	std::vector<std::string> processes;

    	std::unordered_map<std::string, size_t> runtime_array_sizes;

    	void update_id();
    };

    //class ShaderSource
    //{
    //  public:
    //	ShaderSource() = default;
    //
    //	ShaderSource(const std::string &filename);
    //
    //	size_t get_id() const;
    //
    //	const std::string &get_filename() const;
    //
    //	void set_source(const std::string &source);
    //
    //	const std::string &get_source() const;
    //
    //  private:
    //	size_t id;
    //
    //	std::string filename;
    //
    //	std::string source;
    //};

    /**
     * @brief Contains shader code, with an entry point, for a specific shader stage.
     * It is needed by a PipelineLayout to create a Pipeline.
     * ShaderModule can do auto-pairing between shader code and textures.
     * The low level code can change bindings, just keeping the name of the texture.
     * Variants for each texture are also generated, such as HAS_BASE_COLOR_TEX.
     * It works similarly for attribute locations. A current limitation is that only set 0
     * is considered. Uniform buffers are currently hardcoded as well.
     */
    class ShaderModule
    {
    	DISABLE_COPY_AND_ASSIGN(ShaderModule);

    public:
    	ShaderModule(
    		//std::shared_ptr<Device>& device,
    	    Shader::Stage stage,
    		const std::string &   glsl_source,
    		const std::string &   entry_point = "main",
    		const ShaderVariant & shader_variant = {});
    
    	~ShaderModule();
    	//ShaderModule(ShaderModule &&other);

    	static std::shared_ptr<ShaderModule> Make(
    		//std::shared_ptr<Device>& device,
    		Shader::Stage stage,
    		const std::string& glsl_source,
    		const std::string& entry_point = "main",
    		const ShaderVariant& shader_variant = {}
    	) {
    		return std::make_shared<ShaderModule>(stage, glsl_source, entry_point, shader_variant);
    	}

    	ShaderModule &operator=(ShaderModule &&) = delete;

    	bool IsValid() const {return _isValid;}

    	//const VkShaderModule& Handle() const { return _mod; }

    	//VkPipelineShaderStageCreateInfo ShaderStageCreateInfo() const
    	//{
    	//	VkPipelineShaderStageCreateInfo info{};
    	//	info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    	//	info.stage = stage;
    	//	info.module = Handle();
    	//	info.pName = entry_point.c_str();
    	//	return info;
    	//}

    	size_t GetID() const {return id;}

    	Shader::Stage GetStage() const{return stage;}

    	const std::string& GetEntryPoint() const{return entry_point;}

    	const std::vector<ShaderResource>& GetResources() const{return resources;}

    	const std::string& GetInfoLog() const{return info_log;}

    	const std::vector<uint32_t>& GetBinary() const {return spirv;}

    	inline const std::string &get_debug_name() const
    	{
    		return debug_name;
    	}

    	inline void set_debug_name(const std::string &name)
    	{
    		debug_name = name;
    	}

    	/**
    	 * @brief Flags a resource to use a different method of being bound to the shader
    	 * @param resource_name The name of the shader resource
    	 * @param resource_mode The mode of how the shader resource will be bound
    	 */
    	void set_resource_mode(const std::string &resource_name, const ShaderResourceMode &resource_mode);

      private:
    	//std::shared_ptr<Device> device;
    	//VkShaderModule _mod;
    	bool _isValid;

    	/// Shader unique id
    	size_t id;

    	/// Stage of the shader (vertex, fragment, etc)
    	Shader::Stage stage;

    	/// Name of the main function
    	std::string entry_point;

    	/// Human-readable name for the shader
    	std::string debug_name;

    	/// Compiled source
    	std::vector<uint32_t> spirv;

    	std::vector<ShaderResource> resources;

    	std::string info_log;
    };


    /// Helper class to generate SPIRV code from GLSL source
    /// A very simple version of the glslValidator application
    class IGLSLCompiler : public RefCntBase{

    public:
        static sp<IGLSLCompiler> Get();

        /**
    	 * @brief Set the glslang target environment to translate to when generating code
    	 * @param target_language The language to translate to
    	 * @param target_language_version The version of the language to translate to
    	 */
        virtual void SetTargetEnvironment(glslang::EShTargetLanguage        target_language,
    	                                  glslang::EShTargetLanguageVersion target_language_version) = 0;

        /**
    	 * @brief Reset the glslang target environment to the default values
    	 */
        virtual void ResetTargetEnvironment() = 0;

        /**
    	 * @brief Compiles GLSL to SPIRV code
    	 * @param stage The Vulkan shader stage flag
    	 * @param glsl_source The GLSL source code to be compiled
    	 * @param entry_point The entrypoint function name of the shader stage
    	 * @param shader_variant The shader variant
    	 * @param[out] spirv The generated SPIRV code
    	 * @param[out] info_log Stores any log messages during the compilation process
    	 */
        virtual bool CompileToSPIRV(Shader::Stage     stage,
    	                            const std::string &            glslSource,
    	                            const std::string &            entryPoint,
    	                            const ShaderVariant &          shaderVariant,
    	                            std::vector<std::uint32_t> &   spirv,
    	                            std::string &                  infoLog) = 0;
    };

    /// Generate a list of shader resource based on SPIRV reflection code, and provided ShaderVariant
    class SPIRVReflection
    {
      public:
    	/// @brief Reflects shader resources from SPIRV code
    	/// @param stage The Vulkan shader stage flag
    	/// @param spirv The SPIRV code of shader
    	/// @param[out] resources The list of reflected shader resources
    	/// @param variant ShaderVariant used for reflection to specify the size of the runtime arrays in Storage Buffers
    	static bool reflect_shader_resources(const std::vector<uint32_t> &spirv,
    	                              std::vector<ShaderResource> &resources,
    	                              const ShaderVariant &        variant);

      private:
    	//void parse_shader_resources(const spirv_cross::Compiler &compiler,
    	//                            VkShaderStageFlagBits        stage,
    	//                            std::vector<ShaderResource> &resources,
    	//                            const ShaderVariant &        variant);

    	//void parse_push_constants(const spirv_cross::Compiler &compiler,
    	//                          VkShaderStageFlagBits        stage,
    	//                          std::vector<ShaderResource> &resources,
    	//                          const ShaderVariant &        variant);

    	//void parse_specialization_constants(const spirv_cross::Compiler &compiler,
    	//                                    VkShaderStageFlagBits        stage,
    	//                                    std::vector<ShaderResource> &resources,
    	//                                    const ShaderVariant &        variant);
    };

#endif


} // namespace alloy


