#include "Importer.h"

#include <cgltf/cgltf.h>
#include <cstdio>

#include <Urho3D/Core/Context.h>
#include <Urho3D/Graphics/Geometry.h>
#include <Urho3D/Graphics/IndexBuffer.h>
#include <Urho3D/Graphics/Model.h>
#include <Urho3D/Graphics/VertexBuffer.h>

using namespace Urho3D;

Importer::Importer(Urho3D::Context* c, const char* path)
    : context(c)
{
    printf("Loading '%s'\n", path);

    cgltf_options options = {};
    cgltf_result result = cgltf_parse_file(&options, path, &data);

    if (result != cgltf_result_success)
    {
        printf("Something went wrong during file parsing.\n");
        return;
    }
    result = cgltf_load_buffers(&options, data, path);
    if (result != cgltf_result_success)
    {
        printf("Something went wrong during file loading.\n");
        return;
    }
    result = cgltf_validate(data);
    if (result != cgltf_result_success)
    {
        printf("Something went wrong after validation.\n");
        return;
    }
    valid = true;
}

Importer::~Importer() { cgltf_free(data); }

bool Importer::ExportMaterials() { return false; }

void RecursiveSceneBuild(cgltf_node* nodes, unsigned int childrenCount)
{
    for (unsigned int i = 0; i < childrenCount; i++)
    {
        printf("A node!\n");
        // Add this node
        auto node = nodes[i];
        // Then add children (if any)
        if (node.children_count)
        {
            RecursiveSceneBuild(*node.children, node.children_count);
        }
    }
}

bool Importer::ExportScene()
{
    RecursiveSceneBuild(data->nodes, data->nodes_count);

    return false;
}

bool CheckConsistency(cgltf_accessor* accessor, cgltf_type type, int size)
{
    return accessor->type == type && size == accessor->buffer_view->size;
}

bool CollectVertices(cgltf_primitive* primitive, Vertices* vertices, int vertices_count)
{
    for (int k = 0; k < primitive->attributes_count; ++k)
    {
        auto attribute = primitive->attributes[k];
        if (vertices_count != attribute.data->count)
        {
            printf("Primitive attribute %s is invalid, attribute count and vertices count are different.\n",
                   attribute.name);
            return false;
        }
        switch (attribute.type)
        {
        case cgltf_attribute_type_position:
        {

            auto check = CheckConsistency(attribute.data, cgltf_type_vec3, vertices_count * 4 * 3);
            if (!check)
            {
                printf("Unable to read data for 'position'.\n");
                return false;
            }

            // Copy actual data
            auto positions = new float[vertices_count * 3];
            cgltf_accessor_unpack_floats(attribute.data, positions, vertices_count * 3);
            for (int l = 0; l < vertices_count; ++l)
            {
                vertices[l].position[0] = positions[l * 3 + 0];
                vertices[l].position[1] = positions[l * 3 + 1];
                vertices[l].position[2] = positions[l * 3 + 2];
            }
            break;
        }
        case cgltf_attribute_type_normal:
        {
            auto check = CheckConsistency(attribute.data, cgltf_type_vec3, vertices_count * 4 * 3);
            if (!check)
            {
                printf("Unable to read data for 'normal'.\n");
                return false;
            }

            // Copy actual data
            auto normals = new float[vertices_count * 3];
            cgltf_accessor_unpack_floats(attribute.data, normals, vertices_count * 3);
            for (int l = 0; l < vertices_count; ++l)
            {
                vertices[l].normal[0] = normals[l * 3 + 0];
                vertices[l].normal[1] = normals[l * 3 + 1];
                vertices[l].normal[2] = normals[l * 3 + 2];
            }
            break;
        }
        case cgltf_attribute_type_texcoord:
        {
            auto check = CheckConsistency(attribute.data, cgltf_type_vec2, vertices_count * 4 * 2);
            if (!check)
            {
                printf("Unable to read data for 'texcoord'.\n");
                return false;
            }

            // TODO: Texture coordinates types may be different
            // Even in the same model
            if (attribute.data->component_type != cgltf_component_type_r_32f)
            {
                printf("TODO: Texture coordinates types may be different.\n");
                return false;
            }
            // Let's just copy to vertices directly
            auto texcoords = new float[vertices_count * 2];
            cgltf_accessor_unpack_floats(attribute.data, texcoords, vertices_count * 2);
            for (int l = 0; l < vertices_count; ++l)
            {
                vertices[l].texCoord[0] = texcoords[l * 2 + 0];
                vertices[l].texCoord[1] = texcoords[l * 2 + 1];
            }
            break;
        }
        default:
            break;
        }
    }
}

bool Importer::ExportMeshes()
{
    auto model = new Model(context);
    ea::vector<SharedPtr<VertexBuffer>> vertexBuffers;
    ea::vector<SharedPtr<IndexBuffer>> indexBuffers;

    ea::vector<VertexElement> elements;
    VertexElement position = {};
    position.index_ = 0;
    position.offset_ = 0;
    position.perInstance_ = true;
    position.semantic_ = VertexElementSemantic::SEM_POSITION;
    position.type_ = VertexElementType::TYPE_VECTOR3;
    elements.push_back(position);
    VertexElement normal = {};
    normal.index_ = 1;
    normal.offset_ = 3 * 4;
    normal.perInstance_ = true;
    normal.semantic_ = VertexElementSemantic::SEM_NORMAL;
    normal.type_ = VertexElementType::TYPE_VECTOR3;
    elements.push_back(normal);
    VertexElement texCoord = {};
    texCoord.index_ = 2;
    texCoord.offset_ = 3 * 4 * 2;
    texCoord.perInstance_ = true;
    texCoord.semantic_ = VertexElementSemantic::SEM_TEXCOORD;
    texCoord.type_ = VertexElementType::TYPE_VECTOR2;
    elements.push_back(texCoord);

    for (int i = 0; i < data->meshes_count; ++i)
    {
        auto mesh = data->meshes[i];
        for (int j = 0; j < mesh.primitives_count; ++j)
        {
            SharedPtr<VertexBuffer> vertexBuffer(new VertexBuffer(context));
            SharedPtr<IndexBuffer> indexBuffer(new IndexBuffer(context));

            auto primitive = mesh.primitives[j];

            auto vertices_count = primitive.attributes[0].data->count;
            auto vertices = new Vertices[vertices_count];
            CollectVertices(&primitive, vertices, vertices_count);

            vertexBuffer->SetSize(vertices_count, elements);
            vertexBuffer->SetData((void*)vertices);
            vertexBuffers.push_back(vertexBuffer);
            for (int k = 0; k < vertices_count; ++k)
            { /*
                 printf("{%f, %f, %f}, {%f, %f, %f}, {%f, %f}\n", vertices[k].position[0], vertices[k].position[1],
                        vertices[k].position[2], vertices[k].normal[0], vertices[k].normal[1], vertices[k].normal[2],
                        vertices[k].texCoord[0], vertices[k].texCoord[1]);
             */
            }
        }
    }
    // model->SetVertexBuffers(vertexBuffers);
}

bool Importer::ExportTextures() { return false; }

bool Importer::ExportAnimations() { return false; }

bool Importer::ExportEverything()
{
    this->ExportMaterials();
    this->ExportScene();
    this->ExportMeshes();
    this->ExportTextures();
    this->ExportAnimations();
    return false;
}