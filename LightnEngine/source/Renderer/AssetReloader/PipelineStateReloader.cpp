#include "PipelineStateReloader.h"

namespace ltn {
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
			char* path = _reloadRequestedShaderPaths[currentRequestIndex];
			recv(sock, path, FILE_PATH_COUNT_MAX, 0);

			//LTN_INFO("Recive request shader reload: %s", path);
			closesocket(sock);
		}
		});
}

void PipelineStateReloader::terminate() {
	_socketReciveThread.detach();
	WSACleanup();
}

void PipelineStateReloader::update() {
	u32 requestCount = _reloadRequestCount;
	_reloadRequestCount = 0;
	for (u32 i = 0; i < requestCount; ++i) {
		LTN_INFO("Execute shader reload: %s", _reloadRequestedShaderPaths[i]);
	}
}
}
