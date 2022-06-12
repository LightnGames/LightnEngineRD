#pragma once
namespace ltn {
class Shader;
class Material;
class MaterialInstance;
class Mesh;
class CommonResource {
public:
	void initialize();
	void terminate();

	static CommonResource* Get();
private:
	Shader* _vertexShader = nullptr;
	Shader* _pixelShader = nullptr;
	Material* _material = nullptr;
	MaterialInstance* _materialInstance = nullptr;
	Mesh* _mesh = nullptr;
};
}