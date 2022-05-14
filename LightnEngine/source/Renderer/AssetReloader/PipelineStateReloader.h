#pragma once
#include <Core/Utility.h>
#include <Renderer/RHI/Rhi.h>
#include <Windows.h>
#include <thread>

namespace ltn {
// Pipeline State ���V�F�[�_�[�p�X�ƕR�Â��ĊǗ�����R���e�i
struct PipelineStateShaderPathContainer {
	static constexpr u32 COUNT_MAX = 256;
	struct ShaderPathInfo {
		u32 _count = 0;
		u64 _shaderPathHashs[2];
	};

	// ��� PipelineState ���������܂�
	u32 findEmptyPipelineStateIndex() const;

	// ������ PipelineState �ƈ�v����z��C���f�b�N�X���������܂�
	u32 findPipelineStateIndex(rhi::PipelineState* pipelineState) const;

	// ������ PipelineState ���������ċ�ɂ��܂�
	bool removePipelineState(rhi::PipelineState* pipelineState);

	// �����̃V�F�[�_�[�n�b�V�����܂� PipelineState ���������Ĉ����̔z��ɏo�͂��܂�
	void collectPipelineState(u64 shaderHash, u16* outIndices, u16& outCount) const;

	u16 _count = 0;
	rhi::PipelineState* _pipelineStates[COUNT_MAX] = {};
	ShaderPathInfo _shaderPathInfos[COUNT_MAX];
};

// Pipeline State ��o�^���ăR���o�[�^�[����V�F�[�_�[�̍X�V�ʒm���󂯎��A�Đ������s���N���X
class PipelineStateReloader {
public:
	struct GraphicsPipelineStateInfos {
		rhi::GraphicsPipelineStateDesc _desc;
		char _shaderPaths[2][FILE_PATH_COUNT_MAX];
	};

	struct ComputePipelineStateInfos {
		rhi::ComputePipelineStateDesc _desc;
		char _shaderPath[FILE_PATH_COUNT_MAX];
	};

	struct GraphicsPipelineStateRegisterDesc {
		rhi::GraphicsPipelineStateDesc _desc;
		const char* _shaderPaths[2];
	};

	struct ComputePipelineStateRegisterDesc {
		rhi::ComputePipelineStateDesc _desc;
		const char* _shaderPath = nullptr;
	};

	void initialize();
	void terminate();
	void update();

	// Graphics Pipeline State �������[�_�[�ɓo�^���܂�
	void registerPipelineState(rhi::PipelineState* pipelineState, GraphicsPipelineStateRegisterDesc& desc);

	// Compute Pipeline State �������[�_�[�ɓo�^���܂�
	void registerPipelineState(rhi::PipelineState* pipelineState, ComputePipelineStateRegisterDesc& desc);

	// Pipeline State �������[�_�[����폜���܂�
	void unregisterPipelineState(rhi::PipelineState* pipelineState);

	static PipelineStateReloader* Get();

private:
	static constexpr u32 RELOAD_RREQUEST_COUNT_MAX = 32;

	std::thread _socketReciveThread;
	SOCKET _socket;

	u32 _reloadRequestCount = 0;
	u64 _reloadRequestShaderPathHashs[RELOAD_RREQUEST_COUNT_MAX];

	PipelineStateShaderPathContainer _graphicsPipelineStateContainer;
	PipelineStateShaderPathContainer _computePipelineStateContainer;
	GraphicsPipelineStateInfos _graphicsPipelineStateInfos[PipelineStateShaderPathContainer::COUNT_MAX];
	ComputePipelineStateInfos _computePipelineStateInfos[PipelineStateShaderPathContainer::COUNT_MAX];
};
}