#pragma once
#include "graphics.h"
#include "file_system.h"
#include "resources.h"

struct StackAllocator;

struct MeshData
{
	float *vertices;
	uint16_t *indices;
	uint32_t indexCount;
	uint32_t vertexCount;
	uint32_t vertexStride;
};

namespace parsers
{
	MeshData GetMeshFromOBJ(File objFile, StackAllocator *stackAllocator);
	AssetDatabase GetAssetDBFromADF(File adfFile, StackAllocator *stackAllocator);
	uint32_t GetVertexInputDescFromShader(File shader, VertexInputDesc vertexInputDescs[VERTEX_SHADER_MAX_INPUT_COUNT]);
}