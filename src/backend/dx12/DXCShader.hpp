#pragma once

//3rd-party headers

//veldrid public headers
#include "veldrid/common/RefCnt.hpp"

#include "veldrid/GraphicsDevice.hpp"
#include "veldrid/Pipeline.hpp"
#include "veldrid/Buffer.hpp"
#include "veldrid/Shader.hpp"

//standard library headers
#include <string>
#include <vector>

//backend specific headers

//platform specific headers
#include <d3d12.h>
#include <dxgi1_4.h> //Guaranteed by DX12
#include <wrl/client.h> // for ComPtr

//Local headers


namespace Veldrid::DXC::priv
{
    //enum ShaderStage {
	//	SHADER_STAGE_VERTEX,
	//	SHADER_STAGE_FRAGMENT,
	//	SHADER_STAGE_TESSELATION_CONTROL,
	//	SHADER_STAGE_TESSELATION_EVALUATION,
	//	SHADER_STAGE_COMPUTE,
	//	SHADER_STAGE_MAX,
	//	SHADER_STAGE_VERTEX_BIT = (1 << SHADER_STAGE_VERTEX),
	//	SHADER_STAGE_FRAGMENT_BIT = (1 << SHADER_STAGE_FRAGMENT),
	//	SHADER_STAGE_TESSELATION_CONTROL_BIT = (1 << SHADER_STAGE_TESSELATION_CONTROL),
	//	SHADER_STAGE_TESSELATION_EVALUATION_BIT = (1 << SHADER_STAGE_TESSELATION_EVALUATION),
	//	SHADER_STAGE_COMPUTE_BIT = (1 << SHADER_STAGE_COMPUTE),
	//};

    /******************************************/
	/**** PIPELINE SPECIALIZATION CONSTANT ****/
	/******************************************/

	enum PipelineSpecializationConstantType {
		PIPELINE_SPECIALIZATION_CONSTANT_TYPE_BOOL,
		PIPELINE_SPECIALIZATION_CONSTANT_TYPE_INT,
		PIPELINE_SPECIALIZATION_CONSTANT_TYPE_FLOAT,
	};

	struct PipelineSpecializationConstant {
		PipelineSpecializationConstantType type;
		uint32_t constant_id;
		union {
			uint32_t int_value;
			float float_value;
			bool bool_value;
		};

		PipelineSpecializationConstant() {
			type = PIPELINE_SPECIALIZATION_CONSTANT_TYPE_BOOL;
			constant_id = 0;
			int_value = 0;
		}
	};

    static const uint32_t ROOT_SIGNATURE_SIZE = 256;
	static const uint32_t PUSH_CONSTANT_SIZE = 128; // Mimicking Vulkan.

	enum {
		// We can only aim to set a maximum here, since depending on the shader
		// there may be more or less root signature free for descriptor tables.
		// Therefore, we'll have to rely on the final check at runtime, when building
		// the root signature structure for a given shader.
		// To be precise, these may be present or not, and their size vary statically:
		// - Push constant (we'll assume this is always present to avoid reserving much
		//   more space for descriptor sets than needed for almost any imaginable case,
		//   given that most shader templates feature push constants).
		// - NIR-DXIL runtime data.
		MAX_UNIFORM_SETS = (ROOT_SIGNATURE_SIZE - PUSH_CONSTANT_SIZE) / sizeof(uint32_t),
	};

	enum ResourceClass {
		RES_CLASS_INVALID,
		RES_CLASS_CBV,
		RES_CLASS_SRV,
		RES_CLASS_UAV,
	};

	struct UniformBindingInfo {
		uint32_t stages = 0; // Actual shader stages using the uniform (0 if totally optimized out).
		ResourceClass res_class = RES_CLASS_INVALID;
		struct RootSignatureLocation {
			uint32_t root_param_idx = UINT32_MAX;
			uint32_t range_idx = UINT32_MAX;
		};
		struct {
			RootSignatureLocation resource;
			RootSignatureLocation sampler;
		} root_sig_locations;
	};

	struct UniformInfo {
		UniformType type = UniformType::UNIFORM_TYPE_MAX;
		bool writable = false;
		int binding = 0;
		int length = 0; // Size of arrays (in total elements), or ubos (in bytes * total elements).

		bool operator!=(const UniformInfo &p_info) const {
			return (binding != p_info.binding || type != p_info.type || writable != p_info.writable || length != p_info.length);
		}

		bool operator<(const UniformInfo &p_info) const {
			if (binding != p_info.binding) {
				return binding < p_info.binding;
			}
			if (type != p_info.type) {
				return type < p_info.type;
			}
			if (writable != p_info.writable) {
				return writable < p_info.writable;
			}
			return length < p_info.length;
		}
	};

	struct UniformSetFormat {
		Vector<UniformInfo> uniform_info;
		bool operator<(const UniformSetFormat &p_format) const {
			uint32_t size = uniform_info.size();
			uint32_t psize = p_format.uniform_info.size();

			if (size != psize) {
				return size < psize;
			}

			const UniformInfo *infoptr = uniform_info.ptr();
			const UniformInfo *pinfoptr = p_format.uniform_info.ptr();

			for (uint32_t i = 0; i < size; i++) {
				if (infoptr[i] != pinfoptr[i]) {
					return infoptr[i] < pinfoptr[i];
				}
			}

			return false;
		}
	};

    //struct ShaderStageSPIRVData {
	//	ShaderStage shader_stage;
	//	std::vector<uint8_t> spir_v;
    //
	//	ShaderStageSPIRVData() {
	//		shader_stage = SHADER_STAGE_VERTEX;
	//	}
	//};
    
	struct Shader {
		//struct ShaderUniformInfo {
		//	UniformInfo info;
		//	UniformBindingInfo binding;
        //
		//	bool operator<(const ShaderUniformInfo &p_info) const {
		//		return *((UniformInfo *)this) < (const UniformInfo &)p_info;
		//	}
		//};
		//struct Set {
		//	std::vector<ShaderUniformInfo> uniforms;
		//	struct {
		//		std::uint32_t resources = 0;
		//		std::uint32_t samplers = 0;
		//	} num_root_params;
		//};

		std::uint32_t vertex_input_mask = 0; // Inputs used, this is mostly for validation.
		std::uint32_t fragment_output_mask = 0;

		std::uint32_t spirv_push_constant_size = 0;
		std::uint32_t dxil_push_constant_size = 0;
		std::uint32_t nir_runtime_data_root_param_idx = UINT32_MAX;

		std::uint32_t compute_local_size[3] = { 0, 0, 0 };

		//struct SpecializationConstant {
		//	PipelineSpecializationConstant constant;
		//	uint64_t stages_bit_offsets[D3D12_BITCODE_OFFSETS_NUM_STAGES];
		//};

		bool is_compute = false;
		//std::vector<Set> sets;
		std::vector<std::uint32_t> set_formats;
		std::vector<SpecializationConstant> specialization_constants;
		std::uint32_t spirv_specialization_constants_ids_mask = 0;
		std::unordered_map<ShaderStage, std::vector<uint8_t>> stages_bytecode;
		std::string name; // Used for debug.

		Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature;
		Microsoft::WRL::ComPtr<ID3D12RootSignatureDeserializer> root_signature_deserializer;
		const D3D12_ROOT_SIGNATURE_DESC *root_signature_desc = nullptr; // Owned by the deserializer.
		std::uint32_t root_signature_crc = 0;
	};

    std::vector<uint8_t> shader_compile_binary_from_spirv(
        const std::vector<uint32_t> &spirv, const std::vector<ShaderStageSPIRVData> &p_spirv, const std::string &p_shader_name);

    uint32_t _shader_patch_dxil_specialization_constant(
			PipelineSpecializationConstantType p_type,
			const void *p_value,
			const uint64_t (&p_stages_bit_offsets)[D3D12_BITCODE_OFFSETS_NUM_STAGES],
			std::unordered_map<ShaderStage, std::vector<uint8_t>> &r_stages_bytecodes,
			bool p_is_first_patch);
	bool _shader_sign_dxil_bytecode(ShaderStage p_stage, std::vector<uint8_t> &r_dxil_blob);

    /*******************/
	/**** PIPELINES ****/
	/*******************/

	Error _apply_specialization_constants(
			const Shader *p_shader,
			const Vector<PipelineSpecializationConstant> &p_specialization_constants,
			HashMap<ShaderStage, Vector<uint8_t>> &r_final_stages_bytecode);
#ifdef DEV_ENABLED
	String _build_pipeline_blob_filename(
			const Vector<uint8_t> &p_blob,
			const Shader *p_shader,
			const Vector<PipelineSpecializationConstant> &p_specialization_constants,
			const String &p_extra_name_suffix = "",
			const String &p_forced_id = "");
	void _save_pso_blob(
			ID3D12PipelineState *p_pso,
			const Shader *p_shader,
			const Vector<PipelineSpecializationConstant> &p_specialization_constants);
	void _save_stages_bytecode(
			const HashMap<ShaderStage, Vector<uint8_t>> &p_stages_bytecode,
			const Shader *p_shader,
			const RID p_shader_rid,
			const Vector<PipelineSpecializationConstant> &p_specialization_constants);
#endif



} // namespace Veldrid::DXC



namespace Veldrid
{
    
    class DXCDevice;

    class DXCShader : public Shader{
        
        DXCDevice* _Dev() const {return reinterpret_cast<DXCDevice*>(dev.get());}

    private:
        //VkShaderModule _shaderModule;
        //std::string _name;

        //VkShaderModule ShaderModule => _shaderModule;

        DXCShader(
            const sp<GraphicsDevice>& dev,
            const Description& desc
        ) : Shader(dev, desc){}
       

    public:
        //const VkShaderModule& GetHandle() const { return _shaderModule; }

        ~DXCShader();

        static sp<Shader> Make(
            const sp<DXCDevice>& dev,
            const Shader::Description& desc,
            const std::vector<std::uint32_t>& spvBinary
        );

    };

} // namespace Veldrid

