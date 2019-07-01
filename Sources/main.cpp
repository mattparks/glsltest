#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <SPIRV/GlslangToSpv.h>
#include <vulkan/vulkan.h>
#include <optional>
#if defined(WIN32)
#include <io.h>
#include <direct.h>
#define GetCurrentDir _getcwd
#else
#define GetCurrentDir getcwd
#endif

std::string ReadFile(const std::string &fileName)
{
	std::ifstream t(fileName);
	std::string str((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
	return str;
}

std::vector<std::string> StringSplit(const std::string &str, const char &sep)
{
	std::vector<std::string> tokens;
	std::string token;
	std::istringstream tokenStream{str};

	while (std::getline(tokenStream, token, sep))
	{
		tokens.push_back(token);
	}

	return tokens;
}

std::string StringReplaceFirst(std::string str, std::string_view token, std::string_view to)
{
	const auto startPos{str.find(token)};

	if (startPos == std::string::npos)
	{
		return str;
	}

	str.replace(startPos, token.length(), to);
	return str;
}

class Uniform
{
public:
	friend std::ostream &operator<<(std::ostream &stream, const Uniform &uniform)
	{
		return stream << "\"binding\": " << uniform.m_binding << ", \"offset\": " << uniform.m_offset << ", \"size\": " << uniform.m_size << ", \"glType\": " << uniform.m_glType;
	}

	int32_t m_binding{-1};
	int32_t m_offset{-1};
	int32_t m_size{-1};
	int32_t m_glType{-1};
	bool m_readOnly{false};
	bool m_writeOnly{false};
	VkShaderStageFlags m_stageFlags{0};
};

class UniformBlock
{
public:
	enum class Type
	{
		None, Uniform, Storage, Push
	};

	std::optional<Uniform> GetUniform(const std::string &name) const
	{
		auto it{m_uniforms.find(name)};

		if (it == m_uniforms.end())
		{
			return std::nullopt;
		}

		return it->second;
	}

	friend std::ostream &operator<<(std::ostream &stream, const UniformBlock &uniformBlock)
	{
		return stream << "\"binding\": " << uniformBlock.m_binding << ", \"size\": " << uniformBlock.m_size << ", \"type\": " << static_cast<uint32_t>(uniformBlock.m_type);
	}

	int32_t m_binding{-1};
	int32_t m_size{-1};
	VkShaderStageFlags m_stageFlags{0};
	Type m_type{Type::Uniform};
	std::map<std::string, Uniform> m_uniforms;
};

class Attribute
{
public:
	friend std::ostream &operator<<(std::ostream &stream, const Attribute &attribute)
	{
		return stream << "\"set\": " << attribute.m_set << ", \"location\": " << attribute.m_location << ", \"size\": " << attribute.m_size << ", \"glType\": " << attribute.m_glType;
	}

	int32_t m_set{-1};
	int32_t m_location{-1};
	int32_t m_size{-1};
	int32_t m_glType{-1};
};

class Constant
{
public:
	friend std::ostream &operator<<(std::ostream &stream, const Constant &constant)
	{
		return stream << "\"binding\": " << constant.m_binding << ", \"size\": " << constant.m_size << ", \"stageFlags\": " << constant.m_stageFlags << ", \"glType\": " << constant.m_glType;
	}

	int32_t m_binding{-1};
	int32_t m_size{-1};
	VkShaderStageFlags m_stageFlags{0};
	int32_t m_glType{-1};
};

class ShaderReflection
{
public:
	friend std::ostream &operator<<(std::ostream &stream, const ShaderReflection &reflection)
	{
		stream << "{\n";

		{
			stream << "  \"attributes\": {\n";

			for (const auto &[attributeName, attribute] : reflection.m_attributes)
			{
				stream << "    \"" << attributeName << "\": {" << attribute << "},\n";
			}

			stream << "  },\n";
		}

		{
			stream << "  \"uniforms\": {\n";

			for (const auto &[uniformName, uniform] : reflection.m_uniforms)
			{
				stream << "    \"" << uniformName << "\": {" << uniform << "},\n";
			}

			stream << "  },\n";
		}

		{
			stream << "  \"uniformBlocks\": {\n";

			for (const auto &[uniformBlockName, uniformBlock] : reflection.m_uniformBlocks)
			{
				stream << "    \"" << uniformBlockName << "\": {" << uniformBlock << "\n";
				stream << "      \"uniforms\": {\n";

				for (const auto &[uniformName, uniform] : uniformBlock.m_uniforms)
				{
					stream << "        \"" << uniformName << "\": {" << uniform << "},\n";
				}

				stream << "      }\n    },\n";
			}

			stream << "  },\n";
		}

		{
			stream << "  \"constants\": {\n";

			for (const auto &[constantName, constant] : reflection.m_constants)
			{
				stream << "    \"" << constantName << "\": {" << constant << "},\n";
			}

			stream << "  }\n";
		}

		return stream << "}\n";
	}

	std::map<std::string, Uniform> m_uniforms;
	std::map<std::string, UniformBlock> m_uniformBlocks;
	std::map<std::string, Attribute> m_attributes;
	std::map<std::string, Constant> m_constants;
};

EShLanguage GetEshLanguage(const VkShaderStageFlags &stageFlag)
{
	switch (stageFlag)
	{
	case VK_SHADER_STAGE_COMPUTE_BIT:
		return EShLangCompute;
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
	default:
		return EShLangCount;
	}
}

TBuiltInResource GetResources()
{
	TBuiltInResource resources{};
	resources.maxLights = 32;
	resources.maxClipPlanes = 6;
	resources.maxTextureUnits = 32;
	resources.maxTextureCoords = 32;
	resources.maxVertexAttribs = 64;
	resources.maxVertexUniformComponents = 4096;
	resources.maxVaryingFloats = 64;
	resources.maxVertexTextureImageUnits = 32;
	resources.maxCombinedTextureImageUnits = 80;
	resources.maxTextureImageUnits = 32;
	resources.maxFragmentUniformComponents = 4096;
	resources.maxDrawBuffers = 32;
	resources.maxVertexUniformVectors = 128;
	resources.maxVaryingVectors = 8;
	resources.maxFragmentUniformVectors = 16;
	resources.maxVertexOutputVectors = 16;
	resources.maxFragmentInputVectors = 15;
	resources.minProgramTexelOffset = -8;
	resources.maxProgramTexelOffset = 7;
	resources.maxClipDistances = 8;
	resources.maxComputeWorkGroupCountX = 65535;
	resources.maxComputeWorkGroupCountY = 65535;
	resources.maxComputeWorkGroupCountZ = 65535;
	resources.maxComputeWorkGroupSizeX = 1024;
	resources.maxComputeWorkGroupSizeY = 1024;
	resources.maxComputeWorkGroupSizeZ = 64;
	resources.maxComputeUniformComponents = 1024;
	resources.maxComputeTextureImageUnits = 16;
	resources.maxComputeImageUniforms = 8;
	resources.maxComputeAtomicCounters = 8;
	resources.maxComputeAtomicCounterBuffers = 1;
	resources.maxVaryingComponents = 60;
	resources.maxVertexOutputComponents = 64;
	resources.maxGeometryInputComponents = 64;
	resources.maxGeometryOutputComponents = 128;
	resources.maxFragmentInputComponents = 128;
	resources.maxImageUnits = 8;
	resources.maxCombinedImageUnitsAndFragmentOutputs = 8;
	resources.maxCombinedShaderOutputResources = 8;
	resources.maxImageSamples = 0;
	resources.maxVertexImageUniforms = 0;
	resources.maxTessControlImageUniforms = 0;
	resources.maxTessEvaluationImageUniforms = 0;
	resources.maxGeometryImageUniforms = 0;
	resources.maxFragmentImageUniforms = 8;
	resources.maxCombinedImageUniforms = 8;
	resources.maxGeometryTextureImageUnits = 16;
	resources.maxGeometryOutputVertices = 256;
	resources.maxGeometryTotalOutputComponents = 1024;
	resources.maxGeometryUniformComponents = 1024;
	resources.maxGeometryVaryingComponents = 64;
	resources.maxTessControlInputComponents = 128;
	resources.maxTessControlOutputComponents = 128;
	resources.maxTessControlTextureImageUnits = 16;
	resources.maxTessControlUniformComponents = 1024;
	resources.maxTessControlTotalOutputComponents = 4096;
	resources.maxTessEvaluationInputComponents = 128;
	resources.maxTessEvaluationOutputComponents = 128;
	resources.maxTessEvaluationTextureImageUnits = 16;
	resources.maxTessEvaluationUniformComponents = 1024;
	resources.maxTessPatchComponents = 120;
	resources.maxPatchVertices = 32;
	resources.maxTessGenLevel = 64;
	resources.maxViewports = 16;
	resources.maxVertexAtomicCounters = 0;
	resources.maxTessControlAtomicCounters = 0;
	resources.maxTessEvaluationAtomicCounters = 0;
	resources.maxGeometryAtomicCounters = 0;
	resources.maxFragmentAtomicCounters = 8;
	resources.maxCombinedAtomicCounters = 8;
	resources.maxAtomicCounterBindings = 1;
	resources.maxVertexAtomicCounterBuffers = 0;
	resources.maxTessControlAtomicCounterBuffers = 0;
	resources.maxTessEvaluationAtomicCounterBuffers = 0;
	resources.maxGeometryAtomicCounterBuffers = 0;
	resources.maxFragmentAtomicCounterBuffers = 1;
	resources.maxCombinedAtomicCounterBuffers = 1;
	resources.maxAtomicCounterBufferSize = 16384;
	resources.maxTransformFeedbackBuffers = 4;
	resources.maxTransformFeedbackInterleavedComponents = 64;
	resources.maxCullDistances = 8;
	resources.maxCombinedClipAndCullDistances = 8;
	resources.maxSamples = 4;
	resources.limits.nonInductiveForLoops = true;
	resources.limits.whileLoops = true;
	resources.limits.doWhileLoops = true;
	resources.limits.generalUniformIndexing = true;
	resources.limits.generalAttributeMatrixVectorIndexing = true;
	resources.limits.generalVaryingIndexing = true;
	resources.limits.generalSamplerIndexing = true;
	resources.limits.generalVariableIndexing = true;
	resources.limits.generalConstantMatrixVectorIndexing = true;
	return resources;
}

int32_t ComputeSize(const glslang::TType *ttype)
{
	// TODO: glslang::TType::computeNumComponents is available but has many issues resolved in this method.
	int32_t components{};

	if (ttype->getBasicType() == glslang::EbtStruct || ttype->getBasicType() == glslang::EbtBlock)
	{
		for (const auto &tl : *ttype->getStruct())
		{
			components += ComputeSize(tl.type);
		}
	}
	else if (ttype->getMatrixCols() != 0)
	{
		components = ttype->getMatrixCols() * ttype->getMatrixRows();
	}
	else
	{
		components = ttype->getVectorSize();
	}

	if (ttype->getArraySizes() != nullptr)
	{
		int32_t arraySize{1};

		for (int32_t d{}; d < ttype->getArraySizes()->getNumDims(); ++d)
		{
			auto dimSize{ttype->getArraySizes()->getDimSize(d)};

			// This only makes sense in paths that have a known array size.
			if (dimSize != glslang::UnsizedArraySize)
			{
				arraySize *= dimSize;
			}
		}

		components *= arraySize;
	}

	return sizeof(float) * components;
}

void LoadUniformBlock(ShaderReflection &shader, const glslang::TProgram &program, const VkShaderStageFlags &stageFlag, const int32_t &i)
{
	auto reflection{program.getUniformBlock(i)};

	for (auto &[uniformBlockName, uniformBlock] : shader.m_uniformBlocks)
	{
		if (uniformBlockName == reflection.name)
		{
			uniformBlock.m_stageFlags |= stageFlag;
			return;
		}
	}

	auto type{UniformBlock::Type::None};

	if (reflection.getType()->getQualifier().storage == glslang::EvqUniform)
	{
		type = UniformBlock::Type::Uniform;
	}

	if (reflection.getType()->getQualifier().storage == glslang::EvqBuffer)
	{
		type = UniformBlock::Type::Storage;
	}

	if (reflection.getType()->getQualifier().layoutPushConstant)
	{
		type = UniformBlock::Type::Push;
	}

	shader.m_uniformBlocks.emplace(reflection.name, UniformBlock{reflection.getBinding(), reflection.size, stageFlag, type});
}

void LoadUniform(ShaderReflection &shader, const glslang::TProgram &program, const VkShaderStageFlags &stageFlag, const int32_t &i)
{
	auto reflection{program.getUniform(i)};

	if (reflection.getBinding() == -1)
	{
		auto splitName{StringSplit(reflection.name, '.')};

		if (splitName.size() > 1)
		{
			for (auto &[uniformBlockName, uniformBlock] : shader.m_uniformBlocks)
			{
				if (uniformBlockName == splitName.at(0))
				{
					uniformBlock.m_uniforms.emplace(StringReplaceFirst(reflection.name, splitName.at(0) + ".", ""),
						Uniform{reflection.getBinding(), reflection.offset, ComputeSize(reflection.getType()), reflection.glDefineType, false, false,
						stageFlag});
					return;
				}
			}
		}
	}

	for (auto &[uniformName, uniform] : shader.m_uniforms)
	{
		if (uniformName == reflection.name)
		{
			uniform.m_stageFlags |= stageFlag;
			return;
		}
	}

	auto &qualifier{reflection.getType()->getQualifier()};
	shader.m_uniforms.emplace(reflection.name, Uniform{reflection.getBinding(), reflection.offset, -1, reflection.glDefineType, qualifier.readonly, qualifier.writeonly, stageFlag});
}

void LoadAttribute(ShaderReflection &shader, const glslang::TProgram &program, const VkShaderStageFlags &stageFlag, const int32_t &i)
{
	auto reflection{program.getPipeInput(i)};

	if (reflection.name.empty())
	{
		return;
	}

	for (const auto &[attributeName, attribute] : shader.m_attributes)
	{
		if (attributeName == reflection.name)
		{
			return;
		}
	}

	auto &qualifier{reflection.getType()->getQualifier()};
	shader.m_attributes.emplace(reflection.name, Attribute{static_cast<int32_t>(qualifier.layoutSet), static_cast<int32_t>(qualifier.layoutLocation), ComputeSize(reflection.getType()), reflection.glDefineType});
}

void Process(ShaderReflection &shader, const std::string &source, const VkShaderStageFlags &moduleFlag)
{
	// Starts converting GLSL to SPIR-V.
	auto language{GetEshLanguage(moduleFlag)};
	glslang::TProgram program;
	glslang::TShader tshader(language);
	auto resources{GetResources()};

	// Enable SPIR-V and Vulkan rules when parsing GLSL.
	auto messages{static_cast<EShMessages>(EShMsgSpvRules | EShMsgVulkanRules | EShMsgDefault | EShMsgDebugInfo)};

	const char *shaderSource = source.c_str();
	tshader.setStrings(&shaderSource, 1);

	tshader.setEnvInput(glslang::EShSourceGlsl, language, glslang::EShClientVulkan, 110);
	tshader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_1);
	tshader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_3);

	auto defaultVersion{glslang::EShTargetVulkan_1_1};

	std::string str;

	if (!tshader.parse(&resources, defaultVersion, true, messages))
	{
		std::cerr << tshader.getInfoLog() << '\n';
		std::cerr << tshader.getInfoDebugLog() << '\n';
		std::cerr << "SPRIV shader parse failed!\n";
	}

	program.addShader(&tshader);

	if (!program.link(messages) || !program.mapIO())
	{
		std::cerr << "Error while linking shader program.\n";
	}

	program.buildReflection();
	//program.dumpReflection();

	for (int32_t i{program.getNumLiveUniformBlocks() - 1}; i >= 0; i--)
	{
		LoadUniformBlock(shader, program, moduleFlag, i);
	}

	for (int32_t i{}; i < program.getNumLiveUniformVariables(); i++)
	{
		LoadUniform(shader, program, moduleFlag, i);
	}

	for (int32_t i{}; i < program.getNumLiveAttributes(); i++)
	{
		LoadAttribute(shader, program, moduleFlag, i);
	}

	// TODO: Load constants.

	glslang::SpvOptions spvOptions;
	spvOptions.generateDebugInfo = true;
	spvOptions.disableOptimizer = true;
	spvOptions.optimizeSize = false;

	spv::SpvBuildLogger logger;
	std::vector<uint32_t> spirv;
	GlslangToSpv(*program.getIntermediate(static_cast<EShLanguage>(language)), spirv, &logger, &spvOptions);

	/*VkShaderModuleCreateInfo shaderModuleCreateInfo{};
	shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModuleCreateInfo.codeSize = spirv.size() * sizeof(uint32_t);
	shaderModuleCreateInfo.pCode = spirv.data();

	VkShaderModule shaderModule;
	VK_CHECK(vkCreateShaderModule(logicalDevice, &shaderModuleCreateInfo, nullptr, &shaderModule));
	return shaderModule;*/
}

int main(int argc, char **argv)
{
	glslang::InitializeProcess();

	ShaderReflection shader;
	Process(shader, ReadFile("Shaders/sample.vert"), VK_SHADER_STAGE_VERTEX_BIT);
	Process(shader, ReadFile("Shaders/sample.frag"), VK_SHADER_STAGE_FRAGMENT_BIT);
	std::cout << shader;

	glslang::FinalizeProcess();

	// Pauses the console.
	std::cout << "Press enter to continue...";
	std::cin.get();
	return EXIT_SUCCESS;
}