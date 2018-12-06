#include <iostream>
#include <string>
#include <fstream>
#include <SPIRV/GlslangToSpv.h>

#if defined(WIN32)
#include <io.h>
#include <direct.h>
#define GetCurrentDir _getcwd
#else
#define GetCurrentDir getcwd
#endif

// I can't be bothered to find Vulkan and link it for a single enum...
#if !defined(VkShaderStageFlags)
typedef enum VkShaderStageFlagBits {
	VK_SHADER_STAGE_VERTEX_BIT = 0x00000001,
	VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT = 0x00000002,
	VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT = 0x00000004,
	VK_SHADER_STAGE_GEOMETRY_BIT = 0x00000008,
	VK_SHADER_STAGE_FRAGMENT_BIT = 0x00000010,
	VK_SHADER_STAGE_COMPUTE_BIT = 0x00000020,
	VK_SHADER_STAGE_ALL_GRAPHICS = 0x0000001F,
	VK_SHADER_STAGE_ALL = 0x7FFFFFFF,
	VK_SHADER_STAGE_RAYGEN_BIT_NV = 0x00000100,
	VK_SHADER_STAGE_ANY_HIT_BIT_NV = 0x00000200,
	VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV = 0x00000400,
	VK_SHADER_STAGE_MISS_BIT_NV = 0x00000800,
	VK_SHADER_STAGE_INTERSECTION_BIT_NV = 0x00001000,
	VK_SHADER_STAGE_CALLABLE_BIT_NV = 0x00002000,
	VK_SHADER_STAGE_TASK_BIT_NV = 0x00000040,
	VK_SHADER_STAGE_MESH_BIT_NV = 0x00000080,
	VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM = 0x7FFFFFFF
} VkShaderStageFlagBits;
typedef uint32_t VkShaderStageFlags;
#endif

std::string ReadFile(const std::string &fileName)
{
	std::ifstream t(fileName);
	std::string str((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
	return str;
}

std::vector<std::string> StringSplit(const std::string &str, const std::string &sep)
{
	char *copy = (char *) malloc(strlen(str.c_str()) + 1);
	strcpy(copy, str.c_str());

	std::vector<std::string> arr;
	char *current = strtok(copy, sep.c_str());

	while (current != nullptr)
	{
		std::string currentS = std::string(current);
		arr.emplace_back(currentS);
		current = strtok(nullptr, sep.c_str());
	}

	free(copy);
	return arr;
}

struct Uniform
{
	std::string m_name;
	int32_t m_binding;
	int32_t m_offset;
	int32_t m_size;
	int32_t m_glType;
	bool m_readOnly;
	bool m_writeOnly;
	VkShaderStageFlags m_stageFlags;

	std::string ToString() const
	{
		std::stringstream result;
		result << "Uniform(name '" << m_name << "', binding " << m_binding << ", offset " << m_offset << ", size " << m_size << ", readonly " << m_readOnly << ", writeonly " << m_writeOnly << ", type " << m_glType << ")";
		return result.str();
	}
};

struct UniformBlock
{
	enum Type
	{
		UNIFORM = 0,
		STORAGE = 1,
		PUSH = 2
	};

	std::string m_name;
	int32_t m_binding;
	int32_t m_size;
	VkShaderStageFlags m_stageFlags;
	Type m_type;
	std::vector<Uniform> m_uniforms;

	std::string ToString() const
	{
		std::stringstream result;
		result << "UniformBlock(name '" << m_name << "', binding " << m_binding << ", size " << m_size << ", type " << m_type << ")";
		return result.str();
	}
};

struct VertexAttribute
{
	std::string m_name;
	int32_t m_set;
	int32_t m_location;
	int32_t m_size;
	int32_t m_glType;

	std::string ToString() const
	{
		std::stringstream result;
		result << "VertexAttribute(name '" << m_name << "', set " << m_set << "', location " << m_location << ", size " << m_size << ", type " << m_glType << ")";
		return result.str();
	}
};

struct ShaderReflection
{
	std::vector<Uniform> m_uniforms;
	std::vector<UniformBlock> m_uniformBlocks;
	std::vector<VertexAttribute> m_vertexAttributes;

	std::string ToString() const
	{
		std::stringstream result;

		if (!m_vertexAttributes.empty())
		{
			result << "Vertex Attributes: \n";

			for (auto &vertexAttribute : m_vertexAttributes)
			{
				result << "  - " << vertexAttribute.ToString() << "\n";
			}
		}

		if (!m_uniforms.empty())
		{
			result << "Uniforms: \n";

			for (auto &uniform : m_uniforms)
			{
				result << "  - " << uniform.ToString() << "\n";
			}
		}

		if (!m_uniformBlocks.empty())
		{
			result << "Uniform Blocks: \n";

			for (auto &uniformBlock : m_uniformBlocks)
			{
				result << "  - " << uniformBlock.ToString() << " \n";

				for (auto &uniform : uniformBlock.m_uniforms)
				{
					result << "	- " << uniform.ToString() << " \n";
				}
			}
		}

		return result.str();
	}
};

void LoadVertexAttribute(ShaderReflection &reflection, const glslang::TProgram &program, const VkShaderStageFlags &stageFlag, const int32_t &i)
{
	for (auto &vertexAttribute : reflection.m_vertexAttributes)
	{
		if (vertexAttribute.m_name == program.getAttributeName(i))
		{
			return;
		}
	}

	auto qualifier = program.getAttributeTType(i)->getQualifier();
	reflection.m_vertexAttributes.emplace_back(VertexAttribute{
		program.getAttributeName(i),
		qualifier.layoutSet,
		program.getAttributeTType(i)->getQualifier().layoutLocation, // FIXME
		static_cast<int32_t>(sizeof(float) * program.getAttributeTType(i)->getVectorSize()), // TODO: Not always of float size.
		program.getAttributeType(i)
	});
}

void LoadUniform(ShaderReflection &reflection, const glslang::TProgram &program, const VkShaderStageFlags &stageFlag, const int32_t &i)
{
	if (program.getUniformBinding(i) == -1)
	{
		auto splitName = StringSplit(program.getUniformName(i), ".");

		if (splitName.size() == 2)
		{
			for (auto &uniformBlock : reflection.m_uniformBlocks)
			{
				if (uniformBlock.m_name == splitName.at(0))
				{
					uniformBlock.m_uniforms.emplace_back(Uniform{
						splitName.at(1),
						program.getUniformBinding(i),
						program.getUniformBufferOffset(i),
						-1, // static_cast<int32_t>(sizeof(float) * program.getUniformArraySize(i) * program.getUniformTType(i)->computeNumComponents()), // TODO: Not always float size.
						program.getUniformType(i),
						false,
						false,
						stageFlag
					});
					return;
				}
			}
		}
	}

	for (auto &uniform : reflection.m_uniforms)
	{
		if (uniform.m_name == program.getUniformName(i))
		{
			uniform.m_stageFlags = uniform.m_stageFlags | stageFlag;
			return;
		}
	}

	auto &qualifier = program.getUniformTType(i)->getQualifier();
	reflection.m_uniforms.emplace_back(Uniform{
		program.getUniformName(i),
		program.getUniformBinding(i),
		program.getUniformBufferOffset(i),
		-1,
		program.getUniformType(i),
		qualifier.readonly,
		qualifier.writeonly,
		stageFlag
	});
}

void LoadUniformBlock(ShaderReflection &reflection, const glslang::TProgram &program, const VkShaderStageFlags &stageFlag, const int32_t &i)
{
	for (auto &uniformBlock : reflection.m_uniformBlocks)
	{
		if (uniformBlock.m_name == program.getUniformBlockName(i))
		{
			uniformBlock.m_stageFlags = uniformBlock.m_stageFlags | stageFlag;
			return;
		}
	}

	UniformBlock::Type type = UniformBlock::Type::UNIFORM;

	if (strcmp(program.getUniformBlockTType(i)->getStorageQualifierString(), "buffer") == 0)
	{
		type = UniformBlock::Type::STORAGE;
	}

	if (program.getUniformBlockTType(i)->getQualifier().layoutPushConstant) // FIXME
	{
		type = UniformBlock::Type::PUSH;
	}

	reflection.m_uniformBlocks.emplace_back(UniformBlock{
		program.getUniformBlockName(i),
		program.getUniformBlockBinding(i),
		program.getUniformBlockSize(i),
		stageFlag,
		type
	});
}

void LoadProgram(ShaderReflection &reflection, const glslang::TProgram &program, const VkShaderStageFlags &stageFlag)
{	// Uniform blocks
	for (int32_t i = program.getNumLiveUniformBlocks() - 1; i >= 0; i--)
	{
		fprintf(stdout, "UB %s: %i\n", program.getUniformBlockName(i),
		        program.getUniformBlockTType(i)->getQualifier().layoutPushConstant); // FIXME
	}

	// Uniforms
	for (int32_t i = 0; i < program.getNumLiveUniformVariables(); i++)
	{
		if (program.getUniformBinding(i) == -1) // Will be part of a uniform block
		{
			fprintf(stdout, "U %s: %i\n", program.getUniformName(i),
				static_cast<int32_t>(sizeof(float) * program.getUniformArraySize(i) * program.getUniformTType(i)->computeNumComponents())); // FIXME
		}
	}

	// Vertex attributes
	for (int32_t i = 0; i < program.getNumLiveAttributes(); i++)
	{
		fprintf(stdout, "VA %s: %i\n", program.getAttributeName(i),
		        program.getAttributeTType(i)->getQualifier().layoutLocation); // FIXME
	}
	/*// Uniform blocks
	for (int32_t i = program.getNumLiveUniformBlocks() - 1; i >= 0; i--)
	{
		LoadUniformBlock(reflection, program, stageFlag, i);
	}

	// Uniforms
	for (int32_t i = 0; i < program.getNumLiveUniformVariables(); i++)
	{
		LoadUniform(reflection, program, stageFlag, i);
	}

	// Vertex attributes
	for (int32_t i = 0; i < program.getNumLiveAttributes(); i++)
	{
		LoadVertexAttribute(reflection, program, stageFlag, i);
	}*/
}

TBuiltInResource GetResources()
{
	TBuiltInResource resources = {};
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

void Process(ShaderReflection &reflection, const std::string &source, const VkShaderStageFlags &stageFlag)
{
	// Starts converting GLSL to SPIR-V.
	EShLanguage language = GetEshLanguage(stageFlag);
	glslang::TProgram program;
	glslang::TShader shader(language);
	TBuiltInResource resources = GetResources();

	// Enable SPIR-V and Vulkan rules when parsing GLSL.
	EShMessages messages = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules | EShMsgDefault);
#if !defined(NDEBUG)
	messages = (EShMessages)(messages | EShMsgDebugInfo);
#endif

	const char *shaderSource = source.c_str();
	shader.setStrings(&shaderSource, 1);

	shader.setEnvInput(glslang::EShSourceGlsl, language, glslang::EShClientVulkan, 110);
	shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_1);
	shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_3);

	const int defaultVersion = glslang::EShTargetOpenGL_450;

	if (!shader.parse(&resources, defaultVersion, false, messages))
	{
		fprintf(stdout, "%s\n", shader.getInfoLog());
		fprintf(stdout, "%s\n", shader.getInfoDebugLog());
		fprintf(stderr, "SPRIV shader compile failed!\n");
	}

	// NOTE: Programs contain one shader, the program will be used to create a shader module.
	program.addShader(&shader);

	if (!program.link(messages) || !program.mapIO())
	{
		fprintf(stderr, "Error while linking shader program.\n");
	}

	program.buildReflection();
	program.dumpReflection();
	LoadProgram(reflection, program, stageFlag);

	glslang::SpvOptions spvOptions;
#if !defined(NDEBUG)
	spvOptions.generateDebugInfo = true;
	spvOptions.disableOptimizer = true;
	spvOptions.optimizeSize = false;
#else
	spvOptions.generateDebugInfo = false;
	spvOptions.disableOptimizer = false;
	spvOptions.optimizeSize = true;
#endif

	spv::SpvBuildLogger logger;
	std::vector<uint32_t> spirv;
	glslang::GlslangToSpv(*program.getIntermediate((EShLanguage)language), spirv, &logger, &spvOptions);
}

int main(int argc, char **argv)
{
	glslang::InitializeProcess();

	char buff[FILENAME_MAX];
	GetCurrentDir(buff, FILENAME_MAX);
	std::string currentDirectory = buff;

	auto reflection = ShaderReflection{};
	Process(reflection, ReadFile(currentDirectory + "/sample.vert"), VK_SHADER_STAGE_VERTEX_BIT);
//	Process(reflection, ReadFile(currentDirectory + "/sample.frag"), VK_SHADER_STAGE_FRAGMENT_BIT);
	fprintf(stdout, "%s\n", reflection.ToString().c_str());

	glslang::FinalizeProcess();

	// Pauses the console.
//	std::cout << "Press enter to continue...";
//	std::cin.get();
	return EXIT_SUCCESS;
}