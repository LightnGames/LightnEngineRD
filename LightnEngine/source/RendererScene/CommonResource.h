#pragma once
namespace ltn {
class Shader;
class Material;
class MaterialInstance;
class MeshGeometry;
class Mesh;
class Texture;
class PipelineSet;
class CommonResource {
public:
	void initialize();
	void terminate();

	const Mesh* getQuadMesh() const { return _mesh; }
	const Material* getGrayMaterial() const { return _grayMaterial; }

	static CommonResource* Get();

private:
	const Shader* _vertexShader = nullptr;
	const Shader* _pixelShader = nullptr;
	const Material* _whiteMaterial = nullptr;
	const Material* _grayMaterial = nullptr;
	const MaterialInstance* _materialInstance = nullptr;
	const MeshGeometry* _meshGeometry = nullptr;
	const Mesh* _mesh = nullptr;
	const Texture* _checkerTexture = nullptr;
	const PipelineSet* _pipelineSet = nullptr;
};
}