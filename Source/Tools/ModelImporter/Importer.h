#pragma once

struct cgltf_data;
namespace Urho3D
{
class Context;
}

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
    bool ExportMaterials(const char* folder);
    bool ExportModels(const char* folder);
};