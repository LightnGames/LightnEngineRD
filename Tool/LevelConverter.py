import yaml, sys, struct, os, xxhash
import AssetDevelopmentConfig
LEVEL_EXT = ".level"

def load_yaml(path):
    try:
        with open(path) as file:
            return yaml.safe_load(file)
            print(obj)
    except Exception as e:
        print('Exception occurred while loading YAML...', file=sys.stderr)
        print(e, file=sys.stderr)
    return None

def convert(file_path):
    level_data = load_yaml(file_path)
    mesh_geometry_paths = level_data['MeshGeometry']
    mesh_paths = level_data['Mesh']
    texture_parameters = level_data['Texture']
    materials = level_data['Material']
    shader_sets = level_data['PipelineSet']
    mesh_instances = level_data['MeshInstance']

    # Mesh パスに応じてソート
    mesh_instances = sorted(mesh_instances, key=lambda x: x['Mesh'])

    file_name = os.path.basename(file_path)
    file_name, file_ext = os.path.splitext(file_name)
    work_dir = os.path.dirname(file_path)
    output_dir = work_dir.replace(AssetDevelopmentConfig.WORK_ROOT, AssetDevelopmentConfig.RESOURCE_ROOT)
    output_name = file_name + LEVEL_EXT
    output_path = os.path.join(output_dir, output_name)

    mesh_geometry_count = len(mesh_geometry_paths)
    mesh_count = len(mesh_paths)
    texture_parameter_count = len(texture_parameters)
    shader_set_count = len(shader_sets)
    material_instance_count = len(materials)
    mesh_instance_count = len(mesh_instances)

    print("convert to : " + output_path)
    print('mesh geometry count : %d' % mesh_geometry_count)
    print('mesh count : %d' % mesh_count)
    print('texture count : %d' % texture_parameter_count)
    print('shader set count : %d' % texture_parameter_count)
    print('material instance count : %d' % material_instance_count)
    print('mesh instance count : %d' % mesh_instance_count)

    if not os.path.exists(output_dir):
        os.mkdir(output_dir)

    string_format = 'utf-8'
    with open(output_path, mode='wb') as fout:
        fout.write('RESH'.encode(string_format))
        fout.write(
            struct.pack('<IIIIII', mesh_geometry_count, mesh_count, texture_parameter_count, shader_set_count, material_instance_count,
                        mesh_instance_count))

        # メッシュ ジオメトリ
        fout.write('MSHG'.encode(string_format))
        for mesh_path in mesh_geometry_paths:
            fout.write(struct.pack('<I', len(mesh_path) + 1))
        for mesh_path in mesh_geometry_paths:
            fout.write(mesh_path.encode(string_format))
            fout.write(b'\0')

        # テクスチャ
        fout.write('TEX '.encode(string_format))
        for texture_parameter in texture_parameters:
            fout.write(struct.pack('<I', len(texture_parameter) + 1))
        for texture_parameter in texture_parameters:
            fout.write(texture_parameter.encode(string_format))
            fout.write(b'\0')

        # shader sets
        """fout.write('SSET'.encode(string_format))
        for shader_set in shader_sets:
            str_length = len(shader_set)
            fout.write(struct.pack('<I', str_length))
            fout.write(shader_set.encode(string_format))"""

        # マテリアル
        fout.write('MAT '.encode(string_format))
        for material_instance in materials:
            fout.write(struct.pack('<I', len(material_instance) + 1))
        for material_instance in materials:
            fout.write(material_instance.encode(string_format))
            fout.write(b'\0')

        # メッシュ
        fout.write('MESH'.encode(string_format))
        for model_path in mesh_paths:
            fout.write(struct.pack('<I', len(model_path) + 1))
        for model_path in mesh_paths:
            fout.write(model_path.encode(string_format))
            fout.write(b'\0')

        # メッシュインスタンス
        fout.write('MESI'.encode(string_format))

        # Mesh の数を計算
        mesh_path_counts = dict()
        for mesh_instance in mesh_instances:
            mesh_path = mesh_instance['Mesh']
            if mesh_path in mesh_path_counts:
                mesh_path_counts[mesh_path].append(mesh_instance)
            else:
                mesh_path_counts[mesh_path] = [mesh_instance]

        fout.write(struct.pack('<I', len(mesh_path_counts)))
        for mesh_path, per_mesh_mesh_instances in mesh_path_counts.items():
            model_path_hash = xxhash.xxh64(mesh_path.encode(string_format)).intdigest()
            fout.write(struct.pack('<Q', model_path_hash))
            fout.write(struct.pack('<I', len(per_mesh_mesh_instances)))
            for mesh_instance in per_mesh_mesh_instances:
                world_matrix_colmns = mesh_instance['WorldMatrix']
                for colmn in world_matrix_colmns:
                    fout.write(struct.pack('<ffff', colmn[0], colmn[1], colmn[2], colmn[3]))
        """for mesh_instance_index in range(mesh_instance_count):
            mesh_instance = mesh_instances[mesh_instance_index]

            # Model
            model_path = mesh_instance['Model']
            model_path_hash = xxhash.xxh64(model_path.encode(string_format)).intdigest()
            fout.write(struct.pack('<Q', model_path_hash))

            # world matrix
            world_matrix_colmns = mesh_instance['WorldMatrix']
            for colmn in world_matrix_colmns:
                fout.write(struct.pack('<ffff', colmn[0], colmn[1], colmn[2], colmn[3]))

            # material instances
            material_instances = mesh_instance['Material']
            material_count = len(material_instances)
            fout.write(struct.pack('<I', material_count))
            for material_instance in material_instances:
                material_index = materials.index(material_instance)
                fout.write(struct.pack('<I', material_index))"""

if __name__ == "__main__":
    args = sys.argv
    level_file_path = args[1]
    convert(level_file_path)
