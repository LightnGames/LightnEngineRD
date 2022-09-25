#include "SkySphere.h"
#include <Core/Memory.h>
#include <Core/CpuTimerManager.h>
#include <RendererScene/Texture.h>

namespace ltn {
namespace {
SkySphereScene g_skySphereScene;
}

void SkySphereScene::initialize() {
	CpuScopedPerf scopedPerf("SkySphereScene");
	{
		VirtualArray::Desc handleDesc = {};
		handleDesc._size = SKY_SPHERE_CAPACITY;
		_skySphereAllocations.initialize(handleDesc);
	}
}

void SkySphereScene::terminate() {
	_skySphereAllocations.terminate();
}

void SkySphereScene::lateUpdate() {
	u32 destroyShaderCount = _skySphereDestroyInfos.getUpdateCount();
	auto destroyShaders = _skySphereDestroyInfos.getObjects();
	for (u32 i = 0; i < destroyShaderCount; ++i) {
		u32 shaderIndex = getSkySphereIndex(destroyShaders[i]);
		_skySphereAllocations.freeAllocation(_skySphereAllocationInfos[shaderIndex]);
		_skySpheres[shaderIndex] = SkySphere();
	}

	_skySphereCreateInfos.reset();
	_skySphereDestroyInfos.reset();
}

const SkySphere* SkySphereScene::createSkySphere(const CreatationDesc& desc) {
	VirtualArray::AllocationInfo allocationInfo = _skySphereAllocations.allocation(1);
	_skySphereAllocationInfos[allocationInfo._offset] = allocationInfo;

	TextureScene* textureScene = TextureScene::Get();
	TextureScene::TextureCreatationDesc textureCreateDesc;
	textureCreateDesc._assetPath = desc._environmentTexturePath;
	const Texture* environmentTexture = textureScene->createTexture(textureCreateDesc);

	textureCreateDesc._assetPath = desc._diffuseTexturePath;
	const Texture* diffuseTexture = textureScene->createTexture(textureCreateDesc);

	textureCreateDesc._assetPath = desc._supecularTexturePath;
	const Texture* supecularTexture = textureScene->createTexture(textureCreateDesc);

	SkySphere* skySphere = &_skySpheres[allocationInfo._offset];
	skySphere->setEnvironmentTextuire(environmentTexture);
	skySphere->setDiffuseTextuire(environmentTexture);
	skySphere->setSupecularTextuire(environmentTexture);

	_skySphereCreateInfos.push(skySphere);
	return skySphere;
}

void SkySphereScene::destroySkySphere(const SkySphere* skySphere) {
	_skySphereDestroyInfos.push(skySphere);

	TextureScene* textureScene = TextureScene::Get();
	textureScene->destroyTexture(skySphere->getEnvironmentTexture());
	textureScene->destroyTexture(skySphere->getDiffuseTexture());
	textureScene->destroyTexture(skySphere->getSupecularTexture());
}

SkySphereScene* SkySphereScene::Get() {
	return &g_skySphereScene;
}
}
