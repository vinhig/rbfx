#include "Importer.h"

#include <cgltf/cgltf.h>
#include <cstdio>

Importer::Importer(const char* path)
{
    printf("Loading '%s'\n", path);
    cgltf_options options = {};
    cgltf_result result = cgltf_parse_file(&options, path, &data);

    if (result != cgltf_result_success)
    {
        printf("Something went wrong during file import.\n");
        return;
    }
    valid = true;
    cgltf_free(data);
}

Importer::~Importer() {}

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

bool Importer::ExportMeshes() { return false; }

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