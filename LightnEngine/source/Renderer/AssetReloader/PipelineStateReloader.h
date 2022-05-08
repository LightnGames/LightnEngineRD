#pragma once
#include <Core/Utility.h>
#include <Windows.h>
#include <thread>

namespace ltn {
class PipelineStateReloader {
public:
	void initialize();
	void terminate();
	void update();

private:
	std::thread _socketReciveThread;
	SOCKET _socket;

	static constexpr u32 RELOAD_RREQUEST_COUNT_MAX = 32;
	u32 _reloadRequestCount = 0;
	char _reloadRequestedShaderPaths[FILE_PATH_COUNT_MAX][RELOAD_RREQUEST_COUNT_MAX];
};

}