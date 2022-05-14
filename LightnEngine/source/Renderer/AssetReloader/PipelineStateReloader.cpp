#include "PipelineStateReloader.h"
#include <Renderer/RenderCore/ReleaseQueue.h>

namespace ltn {
namespace {
PipelineStateReloader g_pipelineStateReloader;

void reloadPipelineState(rhi::PipelineState* pipelineState, const PipelineStateReloader::GraphicsPipelineStateInfos& info) {
	rhi::PipelineState oldPipelineState = *pipelineState;
	ReleaseQueue::Get()->enqueue(oldPipelineState);

	rhi::ShaderBlob vertexShader;
	rhi::ShaderBlob pixelShader;
	vertexShader.initialize(info._shaderPaths[0]);
	pixelShader.initialize(info._shaderPaths[1]);

	rhi::GraphicsPipelineStateDesc psoDesc = info._desc;
	psoDesc._vs = vertexShader.getShaderByteCode();
	psoDesc._ps = pixelShader.getShaderByteCode();
	pipelineState->iniaitlize(psoDesc);

	vertexShader.terminate();
	pixelShader.terminate();
}

void reloadPipelineState(rhi::PipelineState* pipelineState, const PipelineStateReloader::ComputePipelineStateInfos& info) {
	rhi::PipelineState oldPipelineState = *pipelineState;
	ReleaseQueue::Get()->enqueue(oldPipelineState);

	rhi::ShaderBlob computeShader;
	computeShader.initialize(info._shaderPath);

	rhi::ComputePipelineStateDesc psoDesc = info._desc;
	psoDesc._cs = computeShader.getShaderByteCode();
	pipelineState->iniaitlize(psoDesc);

	computeShader.terminate();
}
}

void PipelineStateReloader::initialize() {
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 0), &wsaData);

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(12345);
	addr.sin_addr.S_un.S_addr = INADDR_ANY;

	_socket = socket(addr.sin_family, SOCK_STREAM, 0);
	LTN_ASSERT(_socket != INVALID_SOCKET);

	if (bind(_socket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
		printf("bind: error");
		return;
	}

	if (listen(_socket, 5) == SOCKET_ERROR) {
		printf("listen: error");
		return;
	}

	_socketReciveThread = std::thread([this] {
		while (true) {
			struct sockaddr_in client;
			s32 clientlen = sizeof(client);
			SOCKET sock = accept(_socket, (sockaddr*)&client, &clientlen);

			u32 currentRequestIndex = _reloadRequestCount++;
			char path[FILE_PATH_COUNT_MAX];
			s32 recvLength = recv(sock, path, FILE_PATH_COUNT_MAX, 0);
			if (recvLength != -1) {
				path[recvLength] = '\0';

				_reloadRequestShaderPathHashs[currentRequestIndex] = StrHash64(path);

				LTN_INFO("Recive request shader reload: %s", path);
			}

			closesocket(sock);
		}
		});
}

void PipelineStateReloader::terminate() {
	_socketReciveThread.detach();
	WSACleanup();

	// 解放漏れチェック
	LTN_ASSERT(_graphicsPipelineStateContainer._count == 0);
	LTN_ASSERT(_computePipelineStateContainer._count == 0);
}

void PipelineStateReloader::update() {
	u32 requestCount = _reloadRequestCount;
	_reloadRequestCount = 0;
	for (u32 i = 0; i < requestCount; ++i) {
		u64 shaderPathHash = _reloadRequestShaderPathHashs[i];
		u16 pipelineStateIndices[PipelineStateShaderPathContainer::COUNT_MAX];
		u16 pipelineStateCount;

		_graphicsPipelineStateContainer.collectPipelineState(shaderPathHash, pipelineStateIndices, pipelineStateCount);
		for (u16 psoIndex = 0; psoIndex < _graphicsPipelineStateContainer._count; ++psoIndex) {
			reloadPipelineState(_graphicsPipelineStateContainer._pipelineStates[psoIndex], _graphicsPipelineStateInfos[psoIndex]);
		}

		_computePipelineStateContainer.collectPipelineState(shaderPathHash, pipelineStateIndices, pipelineStateCount);
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

	u32 shaderPathCount = LTN_COUNTOF(desc._shaderPaths);
	{
		GraphicsPipelineStateInfos& info = _graphicsPipelineStateInfos[findIndex];
		for (u32 i = 0; i < shaderPathCount; ++i) {
			memcpy(info._shaderPaths[i], desc._shaderPaths[i], StrLength(desc._shaderPaths[i]));
		}
		info._desc = desc._desc;
	}

	{
		auto& info = _graphicsPipelineStateContainer._shaderPathInfos[findIndex];
		info._count = shaderPathCount;
		for (u32 i = 0; i < shaderPathCount; ++i) {
			info._shaderPathHashs[i] = StrHash64(desc._shaderPaths[i]);
		}
	}

	_graphicsPipelineStateContainer._pipelineStates[findIndex] = pipelineState;
}
void PipelineStateReloader::registerPipelineState(rhi::PipelineState* pipelineState, ComputePipelineStateRegisterDesc& desc) {
	u32 findIndex = _computePipelineStateContainer.findEmptyPipelineStateIndex();
	if (findIndex == -1) {
		findIndex = _graphicsPipelineStateContainer._count++;
	}

	{
		ComputePipelineStateInfos& info = _computePipelineStateInfos[findIndex];
		info._desc = desc._desc;
		memcpy(info._shaderPath, desc._shaderPath, StrLength(desc._shaderPath));
	}

	{
		auto& info = _graphicsPipelineStateContainer._shaderPathInfos[findIndex];
		info._count = 1;
		info._shaderPathHashs[0] = StrHash64(desc._shaderPath);
	}

	_computePipelineStateContainer._pipelineStates[findIndex] = pipelineState;
}
void PipelineStateReloader::unregisterPipelineState(rhi::PipelineState* pipelineState) {
	if (_graphicsPipelineStateContainer.removePipelineState(pipelineState)) {
		return;
	}

	_computePipelineStateContainer.removePipelineState(pipelineState);
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
	if (_count == findIndex + 1) {
		--_count;
	}

	return true;
}

void PipelineStateShaderPathContainer::collectPipelineState(u64 shaderHash, u16* outIndices, u16& outCount) const {
	for (u32 psoIndex = 0; psoIndex < _count; ++psoIndex) {
		const ShaderPathInfo& info = _shaderPathInfos[psoIndex];
		for (u32 shaderIndex = 0; shaderIndex < info._count; ++shaderIndex) {
			if (info._shaderPathHashs[shaderIndex] == shaderHash) {
				outIndices[outCount++] = psoIndex;
			}
		}
	}
}
}
