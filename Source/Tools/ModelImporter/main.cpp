#include <cstdio>

#include "Importer.h"
#include <Urho3D/Core/Context.h>

using namespace Urho3D;

int main(int argc, char const* argv[])
{
    if (argc == 1)
    {
        printf("Okay, it's sad but you didn't specify a path.\n");
        return -1;
    }
    SharedPtr<Context> context = SharedPtr(new Context());
    auto importer = new Importer(context.Get(), argv[1]);

    if (!importer->IsValid())
    {
        printf("Something went wrong during import.\n");
        return -1;
    }
    importer->ExportModels("/home/vincent/gltf-to-mdl");
    importer->ExportMaterials("/home/vincent/gltf-to-mdl/");

    return 0;
}
