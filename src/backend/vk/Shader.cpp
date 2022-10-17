#include "Shader.hpp"


VKBP_DISABLE_WARNINGS()
#include <SPIRV/GLSL.std.450.h>
#include <SPIRV/GlslangToSpv.h>
#include <StandAlone/ResourceLimits.h>
#include <glslang/Include/ShHandle.h>
#include <glslang/OSDependent/osinclude.h>
VKBP_ENABLE_WARNINGS()

#include "common/helpers.h"

namespace vkb
{
/**
 * @brief Pre-compiles project shader files to include header code
 * @param filename The shader file
 * @returns A byte array of the final shader
 */
inline std::vector<std::string> precompile_shader(const std::string &source)
{
	std::vector<std::string> final_file;

	auto lines = split(source, '\n');

	for (auto &line : lines)
	{
		//if (line.find("#include \"") == 0)
		//{
		//	// Include paths are relative to the base shader directory
		//	std::string include_path = line.substr(10);
		//	size_t      last_quote   = include_path.find("\"");
		//	if (!include_path.empty() && last_quote != std::string::npos)
		//	{
		//		include_path = include_path.substr(0, last_quote);
		//	}
        //
		//	auto include_file = precompile_shader(fs::read_shader(include_path));
		//	for (auto &include_file_line : include_file)
		//	{
		//		final_file.push_back(include_file_line);
		//	}
		//}
		//else
		{
			final_file.push_back(line);
		}
	}

	return final_file;
}

inline std::vector<uint8_t> convert_to_bytes(std::vector<std::string> &lines)
{
	std::vector<uint8_t> bytes;

	for (auto &line : lines)
	{
		line += "\n";
		std::vector<uint8_t> line_bytes(line.begin(), line.end());
		bytes.insert(bytes.end(), line_bytes.begin(), line_bytes.end());
	}

	return bytes;
}

ShaderModule::ShaderModule(
	std::shared_ptr<Device>& device,
	VkShaderStageFlagBits stage, 
	const std::string &glsl_source, 
	const std::string &entry_point, 
	const ShaderVariant &shader_variant
) :
    device{device},
    _isValid(false),
    stage{stage},
    entry_point{entry_point}
{
	debug_name = fmt::format("GLSL [variant {:X}] [entrypoint {}]",
	                         shader_variant.get_id(), entry_point);

	// Compiling from GLSL source requires the entry point
	if (entry_point.empty())
	{
		//throw VulkanException{VK_ERROR_INITIALIZATION_FAILED};
        info_log = "Empty entry point";
        return;
	}

	auto &source = glsl_source;//.get_source();

	// Check if application is passing in GLSL source code to compile to SPIR-V
	if (source.empty())
	{
		//throw VulkanException{VK_ERROR_INITIALIZATION_FAILED};
        info_log = "Empty source code";
        return;
	}

	// Precompile source into the final spirv bytecode
	auto glsl_final_source = precompile_shader(source);

	// Compile the GLSL source
	GLSLCompiler glsl_compiler;

	if (!glsl_compiler.compile_to_spirv(stage, convert_to_bytes(glsl_final_source), entry_point, shader_variant, spirv, info_log))
	{
		//LOGE("Shader compilation failed for shader \"{}\"", glsl_source.get_filename());
		LOGE("Shader compilation failed");
		LOGE("{}", info_log);
		//throw VulkanException{VK_ERROR_INITIALIZATION_FAILED};
        return;
	}

	SPIRVReflection spirv_reflection;

	// Reflect all shader resouces
	//if (!spirv_reflection.reflect_shader_resources(stage, spirv, resources, shader_variant))
	//{
	//	throw VulkanException{VK_ERROR_INITIALIZATION_FAILED};
	//}
	spirv_reflection.reflect_shader_resources(stage, spirv, resources, shader_variant);

	// Generate a unique id, determined by source and variant
	std::hash<std::string> hasher{};
	id = hasher(std::string{reinterpret_cast<const char *>(spirv.data()),
	                        reinterpret_cast<const char *>(spirv.data() + spirv.size())});

	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = spirv.size() * sizeof(uint32_t);
	createInfo.pCode = reinterpret_cast<const uint32_t*>(spirv.data());
	
	VK_CHECK(vkCreateShaderModule(device->Handle(), &createInfo, nullptr, &_mod));

    _isValid = true;
}

ShaderModule::~ShaderModule()
{
	vkDestroyShaderModule(device->Handle(), _mod, nullptr);
}

//ShaderModule::ShaderModule(ShaderModule &&other) :
//    //device{other.device},
//    _isValid(other._isValid),
//    id{other.id},
//    stage{other.stage},
//    entry_point{other.entry_point},
//    debug_name{other.debug_name},
//    spirv{other.spirv},
//    resources{other.resources},
//    info_log{other.info_log}
//{
//	other.stage = {};
//}

size_t ShaderModule::get_id() const
{
	return id;
}

VkShaderStageFlagBits ShaderModule::get_stage() const
{
	return stage;
}

const std::string &ShaderModule::get_entry_point() const
{
	return entry_point;
}

const std::vector<ShaderResource> &ShaderModule::get_resources() const
{
	return resources;
}

const std::string &ShaderModule::get_info_log() const
{
	return info_log;
}

const std::vector<uint32_t> &ShaderModule::get_binary() const
{
	return spirv;
}

void ShaderModule::set_resource_mode(const std::string &resource_name, const ShaderResourceMode &resource_mode)
{
	auto it = std::find_if(resources.begin(), resources.end(), [&resource_name](const ShaderResource &resource) { return resource.name == resource_name; });

	if (it != resources.end())
	{
		if (resource_mode == ShaderResourceMode::Dynamic)
		{
			if (it->type == ShaderResourceType::BufferUniform || it->type == ShaderResourceType::BufferStorage)
			{
				it->mode = resource_mode;
			}
			else
			{
				LOGW("Resource `{}` does not support dynamic.", resource_name);
			}
		}
		else
		{
			it->mode = resource_mode;
		}
	}
	else
	{
		LOGW("Resource `{}` not found for shader.", resource_name);
	}
}

ShaderVariant::ShaderVariant(std::string &&preamble, std::vector<std::string> &&processes) :
    preamble{std::move(preamble)},
    processes{std::move(processes)}
{
	update_id();
}

size_t ShaderVariant::get_id() const
{
	return id;
}

void ShaderVariant::add_definitions(const std::vector<std::string> &definitions)
{
	for (auto &definition : definitions)
	{
		add_define(definition);
	}
}

void ShaderVariant::add_define(const std::string &def)
{
	processes.push_back("D" + def);

	std::string tmp_def = def;

	// The "=" needs to turn into a space
	size_t pos_equal = tmp_def.find_first_of("=");
	if (pos_equal != std::string::npos)
	{
		tmp_def[pos_equal] = ' ';
	}

	preamble.append("#define " + tmp_def + "\n");

	update_id();
}

void ShaderVariant::add_undefine(const std::string &undef)
{
	processes.push_back("U" + undef);

	preamble.append("#undef " + undef + "\n");

	update_id();
}

void ShaderVariant::add_runtime_array_size(const std::string &runtime_array_name, size_t size)
{
	if (runtime_array_sizes.find(runtime_array_name) == runtime_array_sizes.end())
	{
		runtime_array_sizes.insert({runtime_array_name, size});
	}
	else
	{
		runtime_array_sizes[runtime_array_name] = size;
	}
}

void ShaderVariant::set_runtime_array_sizes(const std::unordered_map<std::string, size_t> &sizes)
{
	this->runtime_array_sizes = sizes;
}

const std::string &ShaderVariant::get_preamble() const
{
	return preamble;
}

const std::vector<std::string> &ShaderVariant::get_processes() const
{
	return processes;
}

const std::unordered_map<std::string, size_t> &ShaderVariant::get_runtime_array_sizes() const
{
	return runtime_array_sizes;
}

void ShaderVariant::clear()
{
	preamble.clear();
	processes.clear();
	runtime_array_sizes.clear();
	update_id();
}

void ShaderVariant::update_id()
{
	std::hash<std::string> hasher{};
	id = hasher(preamble);
}

//ShaderSource::ShaderSource(const std::string &filename) :
//    filename{filename},
//    source{fs::read_shader(filename)}
//{
//	std::hash<std::string> hasher{};
//	id = hasher(std::string{this->source.cbegin(), this->source.cend()});
//}
//
//size_t ShaderSource::get_id() const
//{
//	return id;
//}
//
//const std::string &ShaderSource::get_filename() const
//{
//	return filename;
//}
//
//void ShaderSource::set_source(const std::string &source_)
//{
//	source = source_;
//	std::hash<std::string> hasher{};
//	id = hasher(std::string{this->source.cbegin(), this->source.cend()});
//}
//
//const std::string &ShaderSource::get_source() const
//{
//	return source;
//}
}        // namespace vkb


namespace vkb
{
namespace
{
inline EShLanguage FindShaderLanguage(VkShaderStageFlagBits stage)
{
	switch (stage)
	{
		case VK_SHADER_STAGE_VERTEX_BIT:
			return EShLangVertex;

		case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
			return EShLangTessControl;

		case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
			return EShLangTessEvaluation;

		case VK_SHADER_STAGE_GEOMETRY_BIT:
			return EShLangGeometry;

		case VK_SHADER_STAGE_FRAGMENT_BIT:
			return EShLangFragment;

		case VK_SHADER_STAGE_COMPUTE_BIT:
			return EShLangCompute;

		case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
			return EShLangRayGen;

		case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
			return EShLangAnyHit;

		case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
			return EShLangClosestHit;

		case VK_SHADER_STAGE_MISS_BIT_KHR:
			return EShLangMiss;

		case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
			return EShLangIntersect;

		case VK_SHADER_STAGE_CALLABLE_BIT_KHR:
			return EShLangCallable;

		default:
			return EShLangVertex;
	}
}
}        // namespace

glslang::EShTargetLanguage        GLSLCompiler::env_target_language         = glslang::EShTargetLanguage::EShTargetNone;
glslang::EShTargetLanguageVersion GLSLCompiler::env_target_language_version = (glslang::EShTargetLanguageVersion) 0;

void GLSLCompiler::set_target_environment(glslang::EShTargetLanguage target_language, glslang::EShTargetLanguageVersion target_language_version)
{
	GLSLCompiler::env_target_language         = target_language;
	GLSLCompiler::env_target_language_version = target_language_version;
}

void GLSLCompiler::reset_target_environment()
{
	GLSLCompiler::env_target_language         = glslang::EShTargetLanguage::EShTargetNone;
	GLSLCompiler::env_target_language_version = (glslang::EShTargetLanguageVersion) 0;
}

bool GLSLCompiler::compile_to_spirv(VkShaderStageFlagBits       stage,
                                    const std::vector<uint8_t> &glsl_source,
                                    const std::string &         entry_point,
                                    const ShaderVariant &       shader_variant,
                                    std::vector<std::uint32_t> &spirv,
                                    std::string &               info_log)
{
	// Initialize glslang library.
	glslang::InitializeProcess();

	EShMessages messages = static_cast<EShMessages>(EShMsgDefault | EShMsgVulkanRules | EShMsgSpvRules);

	EShLanguage language = FindShaderLanguage(stage);
	std::string source   = std::string(glsl_source.begin(), glsl_source.end());

	const char *file_name_list[1] = {""};
	const char *shader_source     = reinterpret_cast<const char *>(source.data());

	glslang::TShader shader(language);
	shader.setStringsWithLengthsAndNames(&shader_source, nullptr, file_name_list, 1);
	shader.setEntryPoint(entry_point.c_str());
	shader.setSourceEntryPoint(entry_point.c_str());
	shader.setPreamble(shader_variant.get_preamble().c_str());
	shader.addProcesses(shader_variant.get_processes());
	if (GLSLCompiler::env_target_language != glslang::EShTargetLanguage::EShTargetNone)
	{
		shader.setEnvTarget(GLSLCompiler::env_target_language, GLSLCompiler::env_target_language_version);
	}

	if (!shader.parse(&glslang::DefaultTBuiltInResource, 100, false, messages))
	{
		info_log = std::string(shader.getInfoLog()) + "\n" + std::string(shader.getInfoDebugLog());
		return false;
	}

	// Add shader to new program object.
	glslang::TProgram program;
	program.addShader(&shader);

	// Link program.
	if (!program.link(messages))
	{
		info_log = std::string(program.getInfoLog()) + "\n" + std::string(program.getInfoDebugLog());
		return false;
	}

	// Save any info log that was generated.
	if (shader.getInfoLog())
	{
		info_log += std::string(shader.getInfoLog()) + "\n" + std::string(shader.getInfoDebugLog()) + "\n";
	}

	if (program.getInfoLog())
	{
		info_log += std::string(program.getInfoLog()) + "\n" + std::string(program.getInfoDebugLog());
	}

	glslang::TIntermediate *intermediate = program.getIntermediate(language);

	// Translate to SPIRV.
	if (!intermediate)
	{
		info_log += "Failed to get shared intermediate code.\n";
		return false;
	}

	spv::SpvBuildLogger logger;

	glslang::GlslangToSpv(*intermediate, spirv, &logger);

	info_log += logger.getAllMessages() + "\n";

	// Shutdown glslang library.
	glslang::FinalizeProcess();

	return true;
}


namespace
{
template <ShaderResourceType T>
inline void read_shader_resource(const spirv_cross::Compiler &compiler,
                                 VkShaderStageFlagBits        stage,
                                 std::vector<ShaderResource> &resources,
                                 const ShaderVariant &        variant)
{
	LOGE("Not implemented! Read shader resources of type.");
}

template <spv::Decoration T>
inline void read_resource_decoration(const spirv_cross::Compiler & /*compiler*/,
                                     const spirv_cross::Resource & /*resource*/,
                                     ShaderResource & /*shader_resource*/,
                                     const ShaderVariant & /* variant */)
{
	LOGE("Not implemented! Read resources decoration of type.");
}

template <>
inline void read_resource_decoration<spv::DecorationLocation>(const spirv_cross::Compiler &compiler,
                                                              const spirv_cross::Resource &resource,
                                                              ShaderResource &             shader_resource,
                                                              const ShaderVariant &        variant)
{
	shader_resource.location = compiler.get_decoration(resource.id, spv::DecorationLocation);
}

template <>
inline void read_resource_decoration<spv::DecorationDescriptorSet>(const spirv_cross::Compiler &compiler,
                                                                   const spirv_cross::Resource &resource,
                                                                   ShaderResource &             shader_resource,
                                                                   const ShaderVariant &        variant)
{
	shader_resource.set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
}

template <>
inline void read_resource_decoration<spv::DecorationBinding>(const spirv_cross::Compiler &compiler,
                                                             const spirv_cross::Resource &resource,
                                                             ShaderResource &             shader_resource,
                                                             const ShaderVariant &        variant)
{
	shader_resource.binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
}

template <>
inline void read_resource_decoration<spv::DecorationInputAttachmentIndex>(const spirv_cross::Compiler &compiler,
                                                                          const spirv_cross::Resource &resource,
                                                                          ShaderResource &             shader_resource,
                                                                          const ShaderVariant &        variant)
{
	shader_resource.input_attachment_index = compiler.get_decoration(resource.id, spv::DecorationInputAttachmentIndex);
}

template <>
inline void read_resource_decoration<spv::DecorationNonWritable>(const spirv_cross::Compiler &compiler,
                                                                 const spirv_cross::Resource &resource,
                                                                 ShaderResource &             shader_resource,
                                                                 const ShaderVariant &        variant)
{
	shader_resource.qualifiers |= ShaderResourceQualifiers::NonWritable;
}

template <>
inline void read_resource_decoration<spv::DecorationNonReadable>(const spirv_cross::Compiler &compiler,
                                                                 const spirv_cross::Resource &resource,
                                                                 ShaderResource &             shader_resource,
                                                                 const ShaderVariant &        variant)
{
	shader_resource.qualifiers |= ShaderResourceQualifiers::NonReadable;
}

inline void read_resource_vec_size(const spirv_cross::Compiler &compiler,
                                   const spirv_cross::Resource &resource,
                                   ShaderResource &             shader_resource,
                                   const ShaderVariant &        variant)
{
	const auto &spirv_type = compiler.get_type_from_variable(resource.id);

	shader_resource.vec_size = spirv_type.vecsize;
	shader_resource.columns  = spirv_type.columns;
}

inline void read_resource_array_size(const spirv_cross::Compiler &compiler,
                                     const spirv_cross::Resource &resource,
                                     ShaderResource &             shader_resource,
                                     const ShaderVariant &        variant)
{
	const auto &spirv_type = compiler.get_type_from_variable(resource.id);

	shader_resource.array_size = spirv_type.array.size() ? spirv_type.array[0] : 1;
}

inline void read_resource_size(const spirv_cross::Compiler &compiler,
                               const spirv_cross::Resource &resource,
                               ShaderResource &             shader_resource,
                               const ShaderVariant &        variant)
{
	const auto &spirv_type = compiler.get_type_from_variable(resource.id);

	size_t array_size = 0;
	if (variant.get_runtime_array_sizes().count(resource.name) != 0)
	{
		array_size = variant.get_runtime_array_sizes().at(resource.name);
	}

	shader_resource.size = to_u32(compiler.get_declared_struct_size_runtime_array(spirv_type, array_size));
}

inline void read_resource_size(const spirv_cross::Compiler &    compiler,
                               const spirv_cross::SPIRConstant &constant,
                               ShaderResource &                 shader_resource,
                               const ShaderVariant &            variant)
{
	auto spirv_type = compiler.get_type(constant.constant_type);

	switch (spirv_type.basetype)
	{
		case spirv_cross::SPIRType::BaseType::Boolean:
		case spirv_cross::SPIRType::BaseType::Char:
		case spirv_cross::SPIRType::BaseType::Int:
		case spirv_cross::SPIRType::BaseType::UInt:
		case spirv_cross::SPIRType::BaseType::Float:
			shader_resource.size = 4;
			break;
		case spirv_cross::SPIRType::BaseType::Int64:
		case spirv_cross::SPIRType::BaseType::UInt64:
		case spirv_cross::SPIRType::BaseType::Double:
			shader_resource.size = 8;
			break;
		default:
			shader_resource.size = 0;
			break;
	}
}

template <>
inline void read_shader_resource<ShaderResourceType::Input>(const spirv_cross::Compiler &compiler,
                                                            VkShaderStageFlagBits        stage,
                                                            std::vector<ShaderResource> &resources,
                                                            const ShaderVariant &        variant)
{
	auto input_resources = compiler.get_shader_resources().stage_inputs;

	for (auto &resource : input_resources)
	{
		ShaderResource shader_resource{};
		shader_resource.type   = ShaderResourceType::Input;
		shader_resource.stages = stage;
		shader_resource.name   = resource.name;

		read_resource_vec_size(compiler, resource, shader_resource, variant);
		read_resource_array_size(compiler, resource, shader_resource, variant);
		read_resource_decoration<spv::DecorationLocation>(compiler, resource, shader_resource, variant);

		resources.push_back(shader_resource);
	}
}

template <>
inline void read_shader_resource<ShaderResourceType::InputAttachment>(const spirv_cross::Compiler &compiler,
                                                                      VkShaderStageFlagBits /*stage*/,
                                                                      std::vector<ShaderResource> &resources,
                                                                      const ShaderVariant &        variant)
{
	auto subpass_resources = compiler.get_shader_resources().subpass_inputs;

	for (auto &resource : subpass_resources)
	{
		ShaderResource shader_resource{};
		shader_resource.type   = ShaderResourceType::InputAttachment;
		shader_resource.stages = VK_SHADER_STAGE_FRAGMENT_BIT;
		shader_resource.name   = resource.name;

		read_resource_array_size(compiler, resource, shader_resource, variant);
		read_resource_decoration<spv::DecorationInputAttachmentIndex>(compiler, resource, shader_resource, variant);
		read_resource_decoration<spv::DecorationDescriptorSet>(compiler, resource, shader_resource, variant);
		read_resource_decoration<spv::DecorationBinding>(compiler, resource, shader_resource, variant);

		resources.push_back(shader_resource);
	}
}

template <>
inline void read_shader_resource<ShaderResourceType::Output>(const spirv_cross::Compiler &compiler,
                                                             VkShaderStageFlagBits        stage,
                                                             std::vector<ShaderResource> &resources,
                                                             const ShaderVariant &        variant)
{
	auto output_resources = compiler.get_shader_resources().stage_outputs;

	for (auto &resource : output_resources)
	{
		ShaderResource shader_resource{};
		shader_resource.type   = ShaderResourceType::Output;
		shader_resource.stages = stage;
		shader_resource.name   = resource.name;

		read_resource_array_size(compiler, resource, shader_resource, variant);
		read_resource_vec_size(compiler, resource, shader_resource, variant);
		read_resource_decoration<spv::DecorationLocation>(compiler, resource, shader_resource, variant);

		resources.push_back(shader_resource);
	}
}

template <>
inline void read_shader_resource<ShaderResourceType::Image>(const spirv_cross::Compiler &compiler,
                                                            VkShaderStageFlagBits        stage,
                                                            std::vector<ShaderResource> &resources,
                                                            const ShaderVariant &        variant)
{
	auto image_resources = compiler.get_shader_resources().separate_images;

	for (auto &resource : image_resources)
	{
		ShaderResource shader_resource{};
		shader_resource.type   = ShaderResourceType::Image;
		shader_resource.stages = stage;
		shader_resource.name   = resource.name;

		read_resource_array_size(compiler, resource, shader_resource, variant);
		read_resource_decoration<spv::DecorationDescriptorSet>(compiler, resource, shader_resource, variant);
		read_resource_decoration<spv::DecorationBinding>(compiler, resource, shader_resource, variant);

		resources.push_back(shader_resource);
	}
}

template <>
inline void read_shader_resource<ShaderResourceType::ImageSampler>(const spirv_cross::Compiler &compiler,
                                                                   VkShaderStageFlagBits        stage,
                                                                   std::vector<ShaderResource> &resources,
                                                                   const ShaderVariant &        variant)
{
	auto image_resources = compiler.get_shader_resources().sampled_images;

	for (auto &resource : image_resources)
	{
		ShaderResource shader_resource{};
		shader_resource.type   = ShaderResourceType::ImageSampler;
		shader_resource.stages = stage;
		shader_resource.name   = resource.name;

		read_resource_array_size(compiler, resource, shader_resource, variant);
		read_resource_decoration<spv::DecorationDescriptorSet>(compiler, resource, shader_resource, variant);
		read_resource_decoration<spv::DecorationBinding>(compiler, resource, shader_resource, variant);

		resources.push_back(shader_resource);
	}
}

template <>
inline void read_shader_resource<ShaderResourceType::ImageStorage>(const spirv_cross::Compiler &compiler,
                                                                   VkShaderStageFlagBits        stage,
                                                                   std::vector<ShaderResource> &resources,
                                                                   const ShaderVariant &        variant)
{
	auto storage_resources = compiler.get_shader_resources().storage_images;

	for (auto &resource : storage_resources)
	{
		ShaderResource shader_resource{};
		shader_resource.type   = ShaderResourceType::ImageStorage;
		shader_resource.stages = stage;
		shader_resource.name   = resource.name;

		read_resource_array_size(compiler, resource, shader_resource, variant);
		read_resource_decoration<spv::DecorationNonReadable>(compiler, resource, shader_resource, variant);
		read_resource_decoration<spv::DecorationNonWritable>(compiler, resource, shader_resource, variant);
		read_resource_decoration<spv::DecorationDescriptorSet>(compiler, resource, shader_resource, variant);
		read_resource_decoration<spv::DecorationBinding>(compiler, resource, shader_resource, variant);

		resources.push_back(shader_resource);
	}
}

template <>
inline void read_shader_resource<ShaderResourceType::Sampler>(const spirv_cross::Compiler &compiler,
                                                              VkShaderStageFlagBits        stage,
                                                              std::vector<ShaderResource> &resources,
                                                              const ShaderVariant &        variant)
{
	auto sampler_resources = compiler.get_shader_resources().separate_samplers;

	for (auto &resource : sampler_resources)
	{
		ShaderResource shader_resource{};
		shader_resource.type   = ShaderResourceType::Sampler;
		shader_resource.stages = stage;
		shader_resource.name   = resource.name;

		read_resource_array_size(compiler, resource, shader_resource, variant);
		read_resource_decoration<spv::DecorationDescriptorSet>(compiler, resource, shader_resource, variant);
		read_resource_decoration<spv::DecorationBinding>(compiler, resource, shader_resource, variant);

		resources.push_back(shader_resource);
	}
}

template <>
inline void read_shader_resource<ShaderResourceType::BufferUniform>(const spirv_cross::Compiler &compiler,
                                                                    VkShaderStageFlagBits        stage,
                                                                    std::vector<ShaderResource> &resources,
                                                                    const ShaderVariant &        variant)
{
	auto uniform_resources = compiler.get_shader_resources().uniform_buffers;

	for (auto &resource : uniform_resources)
	{
		ShaderResource shader_resource{};
		shader_resource.type   = ShaderResourceType::BufferUniform;
		shader_resource.stages = stage;
		shader_resource.name   = resource.name;

		read_resource_size(compiler, resource, shader_resource, variant);
		read_resource_array_size(compiler, resource, shader_resource, variant);
		read_resource_decoration<spv::DecorationDescriptorSet>(compiler, resource, shader_resource, variant);
		read_resource_decoration<spv::DecorationBinding>(compiler, resource, shader_resource, variant);

		resources.push_back(shader_resource);
	}
}

template <>
inline void read_shader_resource<ShaderResourceType::BufferStorage>(const spirv_cross::Compiler &compiler,
                                                                    VkShaderStageFlagBits        stage,
                                                                    std::vector<ShaderResource> &resources,
                                                                    const ShaderVariant &        variant)
{
	auto storage_resources = compiler.get_shader_resources().storage_buffers;

	for (auto &resource : storage_resources)
	{
		ShaderResource shader_resource;
		shader_resource.type   = ShaderResourceType::BufferStorage;
		shader_resource.stages = stage;
		shader_resource.name   = resource.name;

		read_resource_size(compiler, resource, shader_resource, variant);
		read_resource_array_size(compiler, resource, shader_resource, variant);
		read_resource_decoration<spv::DecorationNonReadable>(compiler, resource, shader_resource, variant);
		read_resource_decoration<spv::DecorationNonWritable>(compiler, resource, shader_resource, variant);
		read_resource_decoration<spv::DecorationDescriptorSet>(compiler, resource, shader_resource, variant);
		read_resource_decoration<spv::DecorationBinding>(compiler, resource, shader_resource, variant);

		resources.push_back(shader_resource);
	}
}
}        // namespace

bool SPIRVReflection::reflect_shader_resources(VkShaderStageFlagBits stage, const std::vector<uint32_t> &spirv, std::vector<ShaderResource> &resources, const ShaderVariant &variant)
{
	spirv_cross::CompilerGLSL compiler{spirv};

	auto opts                     = compiler.get_common_options();
	opts.enable_420pack_extension = true;

	compiler.set_common_options(opts);

	parse_shader_resources(compiler, stage, resources, variant);
	parse_push_constants(compiler, stage, resources, variant);
	parse_specialization_constants(compiler, stage, resources, variant);

	return true;
}

void SPIRVReflection::parse_shader_resources(const spirv_cross::Compiler &compiler, VkShaderStageFlagBits stage, std::vector<ShaderResource> &resources, const ShaderVariant &variant)
{
	read_shader_resource<ShaderResourceType::Input>(compiler, stage, resources, variant);
	read_shader_resource<ShaderResourceType::InputAttachment>(compiler, stage, resources, variant);
	read_shader_resource<ShaderResourceType::Output>(compiler, stage, resources, variant);
	read_shader_resource<ShaderResourceType::Image>(compiler, stage, resources, variant);
	read_shader_resource<ShaderResourceType::ImageSampler>(compiler, stage, resources, variant);
	read_shader_resource<ShaderResourceType::ImageStorage>(compiler, stage, resources, variant);
	read_shader_resource<ShaderResourceType::Sampler>(compiler, stage, resources, variant);
	read_shader_resource<ShaderResourceType::BufferUniform>(compiler, stage, resources, variant);
	read_shader_resource<ShaderResourceType::BufferStorage>(compiler, stage, resources, variant);
}

void SPIRVReflection::parse_push_constants(const spirv_cross::Compiler &compiler, VkShaderStageFlagBits stage, std::vector<ShaderResource> &resources, const ShaderVariant &variant)
{
	auto shader_resources = compiler.get_shader_resources();

	for (auto &resource : shader_resources.push_constant_buffers)
	{
		const auto &spivr_type = compiler.get_type_from_variable(resource.id);

		std::uint32_t offset = std::numeric_limits<std::uint32_t>::max();

		for (auto i = 0U; i < spivr_type.member_types.size(); ++i)
		{
			auto mem_offset = compiler.get_member_decoration(spivr_type.self, i, spv::DecorationOffset);

			offset = std::min(offset, mem_offset);
		}

		ShaderResource shader_resource{};
		shader_resource.type   = ShaderResourceType::PushConstant;
		shader_resource.stages = stage;
		shader_resource.name   = resource.name;
		shader_resource.offset = offset;

		read_resource_size(compiler, resource, shader_resource, variant);

		shader_resource.size -= shader_resource.offset;

		resources.push_back(shader_resource);
	}
}

void SPIRVReflection::parse_specialization_constants(const spirv_cross::Compiler &compiler, VkShaderStageFlagBits stage, std::vector<ShaderResource> &resources, const ShaderVariant &variant)
{
	auto specialization_constants = compiler.get_specialization_constants();

	for (auto &resource : specialization_constants)
	{
		auto &spirv_value = compiler.get_constant(resource.id);

		ShaderResource shader_resource{};
		shader_resource.type        = ShaderResourceType::SpecializationConstant;
		shader_resource.stages      = stage;
		shader_resource.name        = compiler.get_name(resource.id);
		shader_resource.offset      = 0;
		shader_resource.constant_id = resource.constant_id;

		read_resource_size(compiler, spirv_value, shader_resource, variant);

		resources.push_back(shader_resource);
	}
}
}        // namespace vkb