#include "Importer.h"

#include <cgltf/cgltf.h>
#include <cstdio>

#include <Urho3D/Core/Context.h>
#include <Urho3D/Graphics/Geometry.h>
#include <Urho3D/Graphics/IndexBuffer.h>
#include <Urho3D/Graphics/Material.h>
#include <Urho3D/Graphics/Model.h>
#include <Urho3D/Graphics/ModelView.h>
#include <Urho3D/Resource/XMLArchive.h>

using namespace Urho3D;

Importer::Importer(Context* c, const char* path)
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

bool Importer::ExportMaterials(const char* folder)
{
    for (int i = 0; i < data->materials_count; ++i)
    {
        auto material = data->materials[i];
        SharedPtr<Material> urhoMaterial = SharedPtr(new Material(context));

        // Mirroring parameters from gltf to urho material
        if (material.double_sided)
        {
            urhoMaterial->SetCullMode(CullMode::CULL_NONE);
        }
        else
        {
            urhoMaterial->SetCullMode(CullMode::CULL_CW);
        }

        if (material.pbr_metallic_roughness.base_color_texture.texture)
        {
            printf("diffuse texture\n");
        }

        if (material.normal_texture.texture)
        {
            printf("material.normal_texture.texture\n");
        }

        if (material.emissive_texture.texture)
        {
        }
    }
    return true;
}

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

/*bool Importer::ExportScene()
{
    RecursiveSceneBuild(data->nodes, data->nodes_count);

    return false;
}*/

bool CheckConsistency(cgltf_accessor* accessor, cgltf_type type, int size)
{
    return accessor->type == type && size == accessor->buffer_view->size;
}

bool CollectVertices(cgltf_primitive* primitive, ModelVertex* vertices, int vertices_count)
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
                vertices[l].position_ = Vector4(positions[l * 3 + 0], positions[l * 3 + 1], positions[l * 3 + 2], 1.0);
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
                vertices[l].normal_ = Vector4(normals[l * 3 + 0], normals[l * 3 + 1], normals[l * 3 + 2], 1.0);
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
                vertices[l].uv_[0] = Vector4(texcoords[l * 2 + 0], texcoords[l * 2 + 1], 0.0f, 0.0f);
            }
            break;
        }
        default:
            break;
        }
    }
}

bool Importer::ExportModels(const char* out)
{
    for (int i = 0; i < data->meshes_count; ++i)
    {
        auto model = new ModelView(context);
        ModelVertexFormat format = {};
        format.position_ = VertexElementType::TYPE_VECTOR3;
        format.normal_ = VertexElementType::TYPE_VECTOR3;
        format.uv_[0] = VertexElementType::TYPE_VECTOR2;
        model->SetVertexFormat(format);

        eastl::vector<GeometryView> geometries;
        auto mesh = data->meshes[i];
        for (int j = 0; j < mesh.primitives_count; ++j)
        {
            auto primitive = mesh.primitives[j];

            GeometryLODView lodView = {};
            lodView.lodDistance_ = 0.0f;

            // Load vertices for this primitive
            // Considering there is one vertex buffer and one index buffer per geometry
            auto vertices_count = primitive.attributes[0].data->count;
            ea::vector<ModelVertex> vertices;
            vertices.resize(vertices_count);
            CollectVertices(&primitive, vertices.data(), vertices_count);
            printf("Loaded %d vertices.\n", vertices.size());

            lodView.vertices_ = vertices;
            for (auto& vertex : lodView.vertices_)
            {
                printf("P:{%f, %f, %f}\n", vertex.position_.x_, vertex.position_.y_, vertex.position_.z_);
                printf("N:{%f, %f, %f}\n", vertex.normal_.x_, vertex.normal_.y_, vertex.normal_.z_);
                printf("T:{%f, %f}\n", vertex.uv_[0].x_, vertex.uv_[0].y_);
            }

            // Load indices
            auto indices_count = primitive.indices->count;
            ea::vector<unsigned int> indices;
            indices.resize(indices_count);
            for (int k = 0; k < indices_count; ++k)
            {
                cgltf_accessor_read_uint(primitive.indices, k, &indices.data()[k], sizeof(unsigned int));
            }

            printf("Loaded %d indices.\n", vertices.size());
            for (auto& index : indices)
            {
                printf("%d, ", index);
            }
            printf("\n");
            lodView.indices_ = indices;

            GeometryView primView = {};
            primView.lods_.push_back(lodView);

            geometries.push_back(primView);
        }
        model->SetGeometries(geometries);

        auto m = model->ExportModel();

        auto vertexBuffers = m->GetVertexBuffers();

        printf("Saving to %s.\n", out);
        if (mesh.name)
        {
            m->SaveFile(eastl::string(out) + mesh.name + ".mdl");
        }
        else
        {
            m->SaveFile(eastl::string(out) + "no-name-" + i + ".mdl");
        }
    }
}