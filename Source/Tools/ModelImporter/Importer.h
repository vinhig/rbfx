#pragma once

struct cgltf_data;

class Importer
{
private:
    bool valid{false};

    cgltf_data* data{nullptr};

public:
    Importer(const char* path);
    ~Importer();

    bool IsValid() const { return valid; }
    bool ExportMaterials();
    bool ExportScene();
    bool ExportMeshes();
    bool ExportTextures();
    bool ExportAnimations();
    bool ExportEverything();
};