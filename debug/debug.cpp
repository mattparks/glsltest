#include <string>
#include <SPIRV/GlslangToSpv.h>

std::string ReadFile(const std::string &fileName);
TBuiltInResource GetResources();

void Process(const std::string &source, const EShLanguage &language)
{
	glslang::TProgram program;
	glslang::TShader shader(language);
	TBuiltInResource resources = GetResources();

	EShMessages messages = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules | EShMsgDefault);

	const char *shaderSource = source.c_str();
	shader.setStrings(&shaderSource, 1);

	shader.setEnvInput(glslang::EShSourceGlsl, language, glslang::EShClientVulkan, 110);
	shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_1);
	shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_3);

	if (!shader.parse(&resources, glslang::EShTargetOpenGL_450, false, messages))
	{
		fprintf(stdout, "%s\n", shader.getInfoLog());
		fprintf(stdout, "%s\n", shader.getInfoDebugLog());
		fprintf(stderr, "SPRIV shader compile failed!\n");
	}

	program.addShader(&shader);

	if (!program.link(messages) || !program.mapIO())
	{
		fprintf(stderr, "Error while linking shader program.\n");
	}

	program.buildReflection();
	program.dumpReflection();
	LogProgram(program);
}

void LogProgram(const glslang::TProgram &program)
{
	// Uniform blocks
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
}

int main(int argc, char **argv)
{
	glslang::InitializeProcess();
	// Switched between vert and frag for output.
	Process(ReadFile("sample.vert"), EShLangVertex);
//	Process(ReadFile("sample.frag"), EShLangFragment);
	glslang::FinalizeProcess();
	return EXIT_SUCCESS;
}
