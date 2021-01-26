#include <cstdio>

#include "Importer.h"
#include <Urho3D/Core/Context.h>

int main(int argc, char const* argv[])
{
    if (argc == 1)
    {
        printf("Okay, it's sad but you didn't specify a path.\n");
        return -1;
    }
    auto importer = new Importer(new Urho3D::Context(), argv[1]);
    if (!importer->IsValid())
    {
        printf("Something went wrong during import.\n");
        return -1;
    }
    importer->ExportMeshes();

    return 0;
}
