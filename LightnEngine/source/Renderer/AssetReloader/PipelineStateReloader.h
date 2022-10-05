#pragma once
#include <Core/Utility.h>
#include <Renderer/RHI/Rhi.h>
#include <Windows.h>
#include <thread>

namespace ltn {
// Pipeline State をシェーダーパスと紐づけて管理するコンテナ
struct PipelineStateShaderPathContainer {
	static constexpr u32 COUNT_MAX = 256;

	// 空の PipelineState を検索します
	u32 findEmptyPipelineStateIndex() const;

	// 引数の PipelineState と一致する配列インデックスを検索します
	u32 findPipelineStateIndex(rhi::PipelineState* pipelineState) const;

	// 引数の PipelineState を検索して空にします
	bool removePipelineState(rhi::PipelineState* pipelineState);

	u16 _count = 0;
	rhi::PipelineState* _pipelineStates[COUNT_MAX] = {};
};

// Pipeline State を登録してコンバーターからシェーダーの更新通知を受け取り、再生成を行うクラス
class PipelineStateReloader {
public:
	struct GraphicsPipelineStateRegisterDesc {
		rhi::GraphicsPipelineStateDesc _desc;
		u64 _shaderPathHashes[2] = {};
	};

	struct ComputePipelineStateRegisterDesc {
		rhi::ComputePipelineStateDesc _desc;
		u64 _shaderPathHash = 0;
	};

	void initialize();
	void terminate();
	void update();

	// Graphics Pipeline State をリローダーに登録します
	void registerPipelineState(rhi::PipelineState* pipelineState, GraphicsPipelineStateRegisterDesc& desc);

	// Compute Pipeline State をリローダーに登録します
	void registerPipelineState(rhi::PipelineState* pipelineState, ComputePipelineStateRegisterDesc& desc);

	// Pipeline State をリローダーから削除します
	void unregisterPipelineState(rhi::PipelineState* pipelineState);

	static PipelineStateReloader* Get();

private:
	static constexpr u32 RELOAD_RREQUEST_COUNT_MAX = 32;

	//std::thread _socketReciveThread;
	//SOCKET _socket;

	u32 _reloadRequestCount = 0;
	u64 _reloadRequestShaderPathHashs[RELOAD_RREQUEST_COUNT_MAX];

	PipelineStateShaderPathContainer _graphicsPipelineStateContainer;
	PipelineStateShaderPathContainer _computePipelineStateContainer;
	GraphicsPipelineStateRegisterDesc _graphicsPipelineStateInfos[PipelineStateShaderPathContainer::COUNT_MAX];
	ComputePipelineStateRegisterDesc _computePipelineStateInfos[PipelineStateShaderPathContainer::COUNT_MAX];
};
}