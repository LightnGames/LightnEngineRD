#pragma once
namespace ltn {
class VramUpdater {
public:
	void initialize();
	void terminate();

	static VramUpdater* Get();
private:
};
}