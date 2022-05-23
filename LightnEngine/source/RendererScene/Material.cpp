#include "Material.h"
#include <Core/Memory.h>

namespace ltn {
namespace {
MaterialScene g_materialScene;
}
void MaterialScene::initialize() {
    _materials = Memory::allocObjects<Material>(MATERIAL_COUNT_MAX);
    _materialInstances = Memory::allocObjects<MaterialInstance>(MATERIAL_INSTANCE_COUNT_MAX);
}

void MaterialScene::terminate() {
    Memory::freeObjects(_materials);
    Memory::freeObjects(_materialInstances);
}

void MaterialScene::lateUpdate() {
}

MaterialScene* MaterialScene::Get() {
    return &g_materialScene;
}
}
