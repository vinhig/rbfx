#pragma once

struct cgltf_data;
#include <Urho3D/Core/Context.h>

using namespace Urho3D;

class Importer
{
private:
    bool valid{false};

    cgltf_data* data{nullptr};
    Context* context{nullptr};

public:
    Importer(Context* context, const char* path);
    ~Importer();

    bool IsValid() const { return valid; }
    bool ExportMaterials(const char* folder);
    bool ExportModels(const char* folder);
};