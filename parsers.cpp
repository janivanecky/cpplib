#include <stdio.h>
#include "logging.h"
#include "parsers.h"
#include "memory.h"

#include <assert.h>

#define MIN(a, b) (a < b ? a : b)
// TODO: Restructure parsers as lexer + synth
// TODO: Better whitespace checking + better end of line checking (unify \r\n and \n)

bool ParseUInt16(char *c, uint16_t *num)
{
	static uint16_t currentNumber = 0;
	static bool parsing = false;

	bool endedParsing = false;

	if (isdigit(*c))
	{
		int16_t num = *c - '0';
		currentNumber = currentNumber * 10 + num;
		parsing = true;
	}
	else if (parsing)
	{
		*num = currentNumber;

		currentNumber = 0;
		parsing = false;
		endedParsing = true;
	}
	if (*c == '/' && !parsing)
	{
		parsing = true;
	}

	return endedParsing;
}

bool ParseFloat(char *c, float *num)
{
	static float currentNumber = 0;
	static float fraction = -1.0f;
	static float sign = 1.0f;
	static bool parsing = false;

	bool endedParsing = false;

	if (isdigit(*c))
	{
		int16_t num = *c - '0';
		if (fraction <= 0.0f)
		{
			currentNumber = 10.0f * currentNumber + (float)num;
			fraction = 0.0f;
		}
		else
		{
			currentNumber = currentNumber + (float)num / fraction;
			fraction *= 10.0f;
		}
		parsing = true;
	}
	else if (*c == '.')
	{
		fraction = 10.0f;
	}
	else if (*c == '-' && !parsing)
	{
		sign = -1.0f;
		parsing = true;
	}
	else if (parsing)
	{
		*num = currentNumber * sign;

		currentNumber = 0;
		fraction = -1.0f;
		sign = 1.0f;
		parsing = false;
		endedParsing = true;
	}

	return endedParsing;
}

template <typename T>
bool ParseNNumbers(char *c, T *storage, uint32_t number, bool parsingFunction(char *, T *))
{
	static uint32_t count = 0;
	bool finished = false;

	if (parsingFunction(c, storage + count))
	{
		count++;
		if (count == number)
		{
			finished = true;
			count = 0;
		}
	}

	return finished;
}

enum OBJParserState
{
	START_OF_LINE,
	PARSE_POSITION,
	PARSE_NORMAL,
	PARSE_TEXCOORD,
	PARSE_FACE,
	GO_TO_NEXT_LINE
};

MeshData parsers::GetMeshFromOBJ(File file, StackAllocator *allocator)
{
	char *fileStart = (char *)file.data;
	uint32_t size = file.size;

	OBJParserState state = START_OF_LINE;

	uint32_t positionCount = 0;
	uint32_t normalCount = 0;
	uint32_t texcoordCount = 0;
	uint32_t indexCount = 0;

	char *ptr = 0;
	uint32_t counter = 0;
	for (counter = 0, ptr = fileStart; ptr && counter < size; counter++, ptr++)
	{
		switch (state)
		{
		case START_OF_LINE:
		{
			if (strncmp(ptr, "vn", 2) == 0)
			{
				normalCount++;
			}
			else if (strncmp(ptr, "vt", 2) == 0)
			{
				texcoordCount++;
			}
			else if (*ptr == 'v')
			{
				positionCount++;
			}
			else if (*ptr == 'f')
			{
				indexCount += 3;
			} 
			state = GO_TO_NEXT_LINE;
		} break;
		case GO_TO_NEXT_LINE:
		{
			if (*ptr == '\n')
			{
				state = START_OF_LINE;
			}
		} break;
		}
	}

	assert(positionCount > 0);
	if (positionCount == 0)
	{
		logging::print_error("No. positions in OBJ file is 0.");
	}

	uint32_t vertexStride = 4;
	if (texcoordCount > 0)
	{
		vertexStride += 2;
	}

	if (normalCount > 0)
	{
		vertexStride += 4;
	}
	float *vertices = memory::alloc_stack<float>(allocator, indexCount * vertexStride);
	uint16_t *indices = memory::alloc_stack<uint16_t>(allocator, indexCount);

	StackAllocatorState stack_state = memory::save_stack_state(allocator);

	// helperIndices store indices for positions/normals/texcoords for each vertex.
	// e.g. if you have f 1/1/1 in obj file, helperIndices will store [1,1,1].
	uint16_t *helperIndices = memory::alloc_stack<uint16_t>(allocator, indexCount * 3);
	float *positions = memory::alloc_stack<float>(allocator, positionCount * 4);
	float *normals = memory::alloc_stack<float>(allocator, normalCount * 4);
	float *texcoords = memory::alloc_stack<float>(allocator, texcoordCount * 2);

	uint16_t *indexPtr = helperIndices;
	float *positionPtr = positions;
	float *normalPtr = normals;
	float *texcoordPtr = texcoords;

	state = START_OF_LINE;
	for (counter = 0, ptr = fileStart; ptr && counter < size; counter++, ptr++)
	{
		switch (state)
		{
		case START_OF_LINE:
		{
			if (strncmp(ptr, "vn", 2) == 0)
			{
				state = PARSE_NORMAL;
			}
			else if (strncmp(ptr, "vt", 2) == 0)
			{
				state = PARSE_TEXCOORD;
			}
			else if (*ptr == 'v')
			{
				state = PARSE_POSITION;
			}
			else if (*ptr == 'f')
			{
				state = PARSE_FACE;
			}
		} break;
		case PARSE_FACE:
		{
			if (ParseNNumbers<uint16_t>(ptr, indexPtr, 9, ParseUInt16))
			{
				indexPtr += 9;
				state = GO_TO_NEXT_LINE;
			}
		} break;
		case PARSE_POSITION:
		{
			if (ParseNNumbers<float>(ptr, positionPtr, 4, ParseFloat))
			{
				positionPtr += 4;
				state = GO_TO_NEXT_LINE;
			}
		} break;
		case PARSE_NORMAL:
		{
			if (ParseNNumbers<float>(ptr, normalPtr, 4, ParseFloat))
			{
				normalPtr += 4;
				state = GO_TO_NEXT_LINE;
			}
		} break;
		case PARSE_TEXCOORD:
		{
			if (ParseNNumbers<float>(ptr, texcoordPtr, 2, ParseFloat))
			{
				texcoordPtr += 2;
				state = GO_TO_NEXT_LINE;
			}
		} break;
		}
		if (state == GO_TO_NEXT_LINE)
		{
			if (*ptr == '\n')
			{
				state = START_OF_LINE;
			}
		}
	}

	// TODO: Check if vertex exists, do not duplicate
	float *verticesPtr = vertices;
	uint16_t *indicesPtr = indices;
	uint16_t *helperIndicesPtr = helperIndices;
	for (uint32_t i = 0; i < indexCount; ++i)
	{
		*(indicesPtr++) = i;

		uint16_t positionIndex = *(helperIndicesPtr) - 1;
		float *position = positions + positionIndex * 4;
		memcpy(verticesPtr, position, sizeof(float) * 4);
		verticesPtr += 4;

		if (texcoordCount > 0)
		{
			uint16_t texcoordIndex = *(helperIndicesPtr + 1) - 1;
			float *texcoord = texcoords + texcoordIndex * 2;
			memcpy(verticesPtr, texcoord, sizeof(float) * 2);
			verticesPtr += 2;
		}

		if (normalCount > 0)
		{
			uint16_t normalIndex = *(helperIndicesPtr + 2) - 1;
			float *normal = normals + normalIndex * 4;
			memcpy(verticesPtr, normal, sizeof(float) * 4);
			verticesPtr += 4;
		}

		helperIndicesPtr += 3;
	}

	MeshData meshData;

	meshData.vertices = vertices;
	meshData.indices = indices;
	meshData.vertexCount = indexCount;
	meshData.indexCount = indexCount;
	meshData.vertexStride = sizeof(float) * vertexStride;

	memory::load_stack_state(allocator, stack_state);

	return meshData;
}

char *assetStrings[] =
{
	"NONE",
	"MESH",
	"VERTEX_SHADER",
	"PIXEL_SHADER",
	"GEOMETRY_SHADER",
	"AUDIO_OGG",
	"FONT"
};

uint32_t AssetTypeFromString(char *string, uint32_t stringLength)
{
	uint32_t type = NONE;

	for (uint32_t assetType = 0; assetType < ARRAYSIZE(assetStrings); ++assetType)
	{
		if (strlen(assetStrings[assetType]) == stringLength && strncmp(string, assetStrings[assetType], stringLength) == 0)
		{
			type = assetType;
			break;
		}
	}

	return type;
}

AssetDatabase parsers::GetAssetDBFromADF(File adfFile, StackAllocator *stackAllocator)
{
	enum State
	{
		PARSING_NAME,
		PARSING_PATH,
		PARSING_TYPE
	};

	char *c = (char *)adfFile.data;
	State state = PARSING_NAME;

	uint32_t assetCount = 0;
	for (uint32_t i = 0; i < adfFile.size; ++i, ++c)
	{
		// Increase asset count in case there's end of line or end of file, AND you're not on a blank line
		if ((*c == '\n' || i == adfFile.size - 1) && *(c - 2) != '\n')
		{
			assetCount++;
		}
	}

	logging::print("Asset count: %d", assetCount);

	sid *keys = memory::alloc_stack<sid>(stackAllocator, assetCount);
	AssetInfo *infos = memory::alloc_stack<AssetInfo>(stackAllocator, assetCount);
	for (uint32_t i = 0; i < assetCount; ++i)
	{
		infos[i].path = memory::alloc_stack<char>(stackAllocator, ASSET_MAX_PATH_LENGTH);
	}

	c = (char *)adfFile.data;
	uint32_t assetCounter = 0;
	for (uint32_t i = 0; i < adfFile.size; ++i, ++c)
	{
		switch (state)
		{
		case PARSING_NAME:
		{
			static uint32_t nameLength = 0;
			if (*c == '\r' || *c == '\n')
			{
				if (nameLength == 0)
				{
					// OK, just an empty line
				}
				else
				{
					logging::print_error("Incorrect ADF format! End of line after asset name.");
				}
			}
			else if (*c == ' ' && *(c - 1) == ')')
			{
				char *nameStart = c - 11;
				nameLength = 10;
				char nameBuffer[ASSET_MAX_NAME_LENGTH];
				if (nameLength > ASSET_MAX_NAME_LENGTH - 1) // Have to account for 0 terminating character
				{
					logging::print_error("Name length read from ADF too long!");
					nameLength = ASSET_MAX_NAME_LENGTH - 1;
				}
				memcpy(nameBuffer, nameStart, MIN(nameLength, ASSET_MAX_NAME_LENGTH - 1));
				nameBuffer[nameLength] = 0;
				keys[assetCounter] = strtoul(nameBuffer, NULL, 16);
				state = PARSING_PATH;
				nameLength = 0;
			}
			else
			{
				nameLength++;
			}
		}
		break;
		case PARSING_PATH:
		{
			static uint32_t pathLength = 0;
			if (*c == '\r' || *c == '\n')
			{
				logging::print_error("Incorrect ADF format! End of line before asset type read.");
			}
			else if (*c == ' ')
			{
				char *pathStart = c - pathLength;
				if (pathLength > ASSET_MAX_PATH_LENGTH - 1) // Have to account for 0 terminating character
				{
					logging::print_error("Path read from ADF too long!");
					pathLength = ASSET_MAX_PATH_LENGTH - 1;
				}
				memcpy(infos[assetCounter].path, pathStart, pathLength);
				infos[assetCounter].path[pathLength] = 0;

				pathLength = 0;
				state = PARSING_TYPE;
			}
			else
			{
				pathLength++;
			}
		}
		break;
		case PARSING_TYPE:
		{
			static uint32_t typeLength = 0;
			if (*c == '\r' || *c == '\n' || i == adfFile.size - 1)
			{
				char *typeStart = c - typeLength;
				if (i == adfFile.size - 1)
				{
					typeLength++;
				}
				infos[assetCounter].type = AssetTypeFromString(typeStart, typeLength);

				typeLength = 0;
				state = PARSING_NAME;

				assetCounter++;
			}
			else
			{
				typeLength++;
			}
		}
		break;
		}
	}

	AssetDatabase result = {};

	result.asset_count = assetCount;
	result.keys = keys;
	result.asset_infos = infos;

	return result;
}

uint32_t parsers::GetVertexInputDescFromShader(File shader, VertexInputDesc vertexInputDescs[VERTEX_SHADER_MAX_INPUT_COUNT])
{
	const char *structName = "VertexInput";
	char *c = (char *)shader.data;
	enum State
	{
		SEARCHING,
		PARSING_TYPE,
		SKIPPING_NAME,
		PARSING_SEMANTIC_NAME
	};

	State state = SEARCHING;
	uint32_t typeLength = 0;
	uint32_t semanticNameLength = 0;
	uint32_t type = 0;

#define SHADER_TYPE_FLOAT4 0
#define SHADER_TYPE_FLOAT2 1

	char *types[] =
	{
		"float4", "float2", "int4"
	};

	DXGI_FORMAT formats[]
	{
		DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R32G32_FLOAT, DXGI_FORMAT_R32G32B32A32_SINT
	};

	uint32_t i = 0;
	uint32_t vertexInputCount = 0;

	while (i < shader.size)
	{
		switch (state)
		{
		case SEARCHING:
		{
			if (strncmp(c, structName, strlen(structName)) == 0)
			{
				state = PARSING_TYPE;
				i += (uint32_t)strlen(structName);
				c += (uint32_t)strlen(structName);
			}
		}
		break;
		case PARSING_TYPE:
		{
			if (*c == ' ' && typeLength > 0)
			{
				for (uint32_t j = 0; j < ARRAYSIZE(types); ++j)
				{
					if (strncmp(c - typeLength, types[j], typeLength) == 0)
					{
						type = j;

						state = SKIPPING_NAME;
						typeLength = 0;

						break;
					}
				}
			}
			else if (isalnum(*c))
			{
				typeLength++;
			}
		}
		break;
		case SKIPPING_NAME:
		{
			if (*c == ':')
			{
				state = PARSING_SEMANTIC_NAME;
			}
		}
		break;
		case PARSING_SEMANTIC_NAME:
		{
			if ((isspace(*c) || *c == ';') && semanticNameLength > 0)
			{
				if (vertexInputCount < VERTEX_SHADER_MAX_INPUT_COUNT)
				{
					vertexInputDescs[vertexInputCount].format = formats[type];
					memcpy(vertexInputDescs[vertexInputCount].semantic_name, c - semanticNameLength, semanticNameLength);
					vertexInputDescs[vertexInputCount].semantic_name[semanticNameLength] = 0;
				}
				vertexInputCount++;

				state = PARSING_TYPE;
				semanticNameLength = 0;
			}
			else if (isalnum(*c) || *c == '_')
			{
				semanticNameLength++;
			}
		}
		}
		++c; ++i;
	}

	return vertexInputCount;
}