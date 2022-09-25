#include "PipelineStateReloader.h"
#include <Renderer/RenderCore/ReleaseQueue.h>
#include <Renderer/RenderCore/GpuShader.h>
#include <RendererScene/Shader.h>

namespace ltn {
namespace {
PipelineStateReloader g_pipelineStateReloader;

void reloadPipelineState(rhi::PipelineState* pipelineState, const PipelineStateReloader::GraphicsPipelineStateRegisterDesc& info) {
	rhi::PipelineState oldPipelineState = *pipelineState;
	ReleaseQueue::Get()->enqueue(oldPipelineState);

	ShaderScene* shaderScene = ShaderScene::Get();
	Shader* vertexShader = shaderScene->findShader(info._shaderPathHashes[0]);
	Shader* pixelShader = shaderScene->findShader(info._shaderPathHashes[1]);

	GpuShaderScene* gpuShaderScene = GpuShaderScene::Get();
	gpuShaderScene->terminateShader(vertexShader);
	gpuShaderScene->terminateShader(pixelShader);
	gpuShaderScene->initializeShader(vertexShader);
	gpuShaderScene->initializeShader(pixelShader);

	rhi::GraphicsPipelineStateDesc psoDesc = info._desc;
	psoDesc._vs = gpuShaderScene->getShader(shaderScene->getShaderIndex(vertexShader))->getShaderByteCode();
	psoDesc._ps = gpuShaderScene->getShader(shaderScene->getShaderIndex(pixelShader))->getShaderByteCode();
	pipelineState->iniaitlize(psoDesc);
}

void reloadPipelineState(rhi::PipelineState* pipelineState, const PipelineStateReloader::ComputePipelineStateRegisterDesc& info) {
	rhi::PipelineState oldPipelineState = *pipelineState;
	ReleaseQueue::Get()->enqueue(oldPipelineState);

	ShaderScene* shaderScene = ShaderScene::Get();
	Shader* computeShader = shaderScene->findShader(info._shaderPathHash);

	GpuShaderScene* gpuShaderScene = GpuShaderScene::Get();
	gpuShaderScene->terminateShader(computeShader);
	gpuShaderScene->initializeShader(computeShader);

	rhi::ComputePipelineStateDesc psoDesc = info._desc;
	psoDesc._cs = gpuShaderScene->getShader(shaderScene->getShaderIndex(computeShader))->getShaderByteCode();
	pipelineState->iniaitlize(psoDesc);
}
}

void PipelineStateReloader::initialize() {
	//WSADATA wsaData;
	//WSAStartup(MAKEWORD(2, 0), &wsaData);

	//struct sockaddr_in addr;
	//addr.sin_family = AF_INET;
	//addr.sin_port = htons(12345);
	//addr.sin_addr.S_un.S_addr = INADDR_ANY;

	//_socket = socket(addr.sin_family, SOCK_STREAM, 0);
	//LTN_ASSERT(_socket != INVALID_SOCKET);

	//if (bind(_socket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
	//	printf("bind: error");
	//	return;
	//}

	//if (listen(_socket, 5) == SOCKET_ERROR) {
	//	printf("listen: error");
	//	return;
	//}

	//_socketReciveThread = std::thread([this] {
	//	while (true) {
	//		struct sockaddr_in client;
	//		s32 clientlen = sizeof(client);
	//		SOCKET sock = accept(_socket, (sockaddr*)&client, &clientlen);

	//		u32 currentRequestIndex = _reloadRequestCount++;
	//		char path[FILE_PATH_COUNT_MAX];
	//		s32 recvLength = recv(sock, path, FILE_PATH_COUNT_MAX, 0);
	//		if (recvLength != -1) {
	//			path[recvLength] = '\0';
	//			_reloadRequestShaderPathHashs[currentRequestIndex] = StrHash64(path);
	//			LTN_INFO("Recive request shader reload: %s", path);
	//		}

	//		closesocket(sock);
	//	}
	//	});
}

void PipelineStateReloader::terminate() {
	//_socketReciveThread.detach();
	//WSACleanup();

	// 解放漏れチェック
	//LTN_ASSERT(_graphicsPipelineStateContainer._count == 0);
	//LTN_ASSERT(_computePipelineStateContainer._count == 0);
}

void PipelineStateReloader::update() {
	u32 requestCount = _reloadRequestCount;
	_reloadRequestCount = 0;
	for (u32 i = 0; i < requestCount; ++i) {
		u64 shaderPathHash = _reloadRequestShaderPathHashs[i];
		u16 pipelineStateIndices[PipelineStateShaderPathContainer::COUNT_MAX];
		u16 pipelineStateCount = 0;

		for (u32 psoIndex = 0; psoIndex < _graphicsPipelineStateContainer._count; ++psoIndex) {
			if (_graphicsPipelineStateInfos[psoIndex]._shaderPathHashes[0] == shaderPathHash) {
				pipelineStateIndices[pipelineStateCount++] = psoIndex;
			}

			if (_graphicsPipelineStateInfos[psoIndex]._shaderPathHashes[1] == shaderPathHash) {
				pipelineStateIndices[pipelineStateCount++] = psoIndex;
			}
		}

		for (u16 psoIndex = 0; psoIndex < _graphicsPipelineStateContainer._count; ++psoIndex) {
			reloadPipelineState(_graphicsPipelineStateContainer._pipelineStates[psoIndex], _graphicsPipelineStateInfos[psoIndex]);
		}

		pipelineStateCount = 0;
		for (u32 psoIndex = 0; psoIndex < _computePipelineStateContainer._count; ++psoIndex) {
			if (_computePipelineStateInfos[psoIndex]._shaderPathHash == shaderPathHash) {
				pipelineStateIndices[pipelineStateCount++] = psoIndex;
			}
		}

		for (u16 psoIndex = 0; psoIndex < _computePipelineStateContainer._count; ++psoIndex) {
			reloadPipelineState(_computePipelineStateContainer._pipelineStates[psoIndex], _computePipelineStateInfos[psoIndex]);
		}
	}
}
void PipelineStateReloader::registerPipelineState(rhi::PipelineState* pipelineState, GraphicsPipelineStateRegisterDesc& desc) {
	u32 findIndex = _graphicsPipelineStateContainer.findEmptyPipelineStateIndex();
	if (findIndex == -1) {
		findIndex = _graphicsPipelineStateContainer._count++;
	}

	_graphicsPipelineStateInfos[findIndex] = desc;
	_graphicsPipelineStateContainer._pipelineStates[findIndex] = pipelineState;
}

void PipelineStateReloader::registerPipelineState(rhi::PipelineState* pipelineState, ComputePipelineStateRegisterDesc& desc) {
	u32 findIndex = _computePipelineStateContainer.findEmptyPipelineStateIndex();
	if (findIndex == -1) {
		findIndex = _computePipelineStateContainer._count++;
	}

	_computePipelineStateInfos[findIndex] = desc;
	_computePipelineStateContainer._pipelineStates[findIndex] = pipelineState;
}

void PipelineStateReloader::unregisterPipelineState(rhi::PipelineState* pipelineState) {
	if (_graphicsPipelineStateContainer.removePipelineState(pipelineState)) {
		return;
	}

	if (_computePipelineStateContainer.removePipelineState(pipelineState)) {
		return;
	}

	LTN_ASSERT(false);
}

PipelineStateReloader* PipelineStateReloader::Get() {
	return &g_pipelineStateReloader;
}

u32 PipelineStateShaderPathContainer::findEmptyPipelineStateIndex() const {
	u32 findIndex = -1;
	for (u32 i = 0; i < _count; ++i) {
		if (_pipelineStates[i] == nullptr) {
			findIndex = i;
			break;
		}
	}
	return findIndex;
}

u32 PipelineStateShaderPathContainer::findPipelineStateIndex(rhi::PipelineState* pipelineState) const {
	u32 findIndex = -1;
	for (u32 i = 0; i < _count; ++i) {
		if (_pipelineStates[i] == pipelineState) {
			findIndex = i;
			break;
		}
	}
	return findIndex;
}

bool PipelineStateShaderPathContainer::removePipelineState(rhi::PipelineState* pipelineState) {
	u32 findIndex = findPipelineStateIndex(pipelineState);
	if (findIndex == -1) {
		return false;
	}

	_pipelineStates[findIndex] = nullptr;
	return true;
}
}
