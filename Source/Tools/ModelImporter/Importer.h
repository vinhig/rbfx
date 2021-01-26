#pragma once

struct cgltf_data;
namespace Urho3D
{
class Context;
}

struct Vertices
{
    float position[3];
    float normal[3];
    float texCoord[2];
};

class Importer
{
private:
    bool valid{false};

    cgltf_data* data{nullptr};
    Urho3D::Context* context{nullptr};

public:
    Importer(Urho3D::Context* context, const char* path);
    ~Importer();

    bool IsValid() const { return valid; }
    bool ExportMaterials();
    bool ExportScene();
    bool ExportMeshes();
    bool ExportTextures();
    bool ExportAnimations();
    bool ExportEverything();
};