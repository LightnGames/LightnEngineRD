import sys, yaml, os.path, subprocess, struct, xxhash, glob
import AssetDevelopmentConfig, RruntimeMessageSender

SMESH_EXT = ".smesh"

def convert(file_path):
    with open(file_path, 'r') as yml:
        static_mesh_data = yaml.load(yml, Loader=yaml.SafeLoader)
        mesh_path = static_mesh_data['Mesh']

        materials = dict()
        if 'Materials' in static_mesh_data:
            materials = static_mesh_data['Materials']

        file_name = os.path.basename(file_path)
        file_name, file_ext = os.path.splitext(file_name)
        work_dir = os.path.dirname(file_path)
        output_dir = work_dir.replace(AssetDevelopmentConfig.WORK_ROOT, AssetDevelopmentConfig.RESOURCE_ROOT)
        output_name = file_name + SMESH_EXT
        output_path = os.path.join(output_dir, output_name)

        print("Materials")
        print(materials)
        print("Output -> %s" % output_path)

        string_format = 'utf-8'
        with open(output_path, mode='wb') as fout:
            mesh_path_hash = xxhash.xxh64(mesh_path.encode(string_format)).intdigest()
            fout.write(struct.pack('<Q', mesh_path_hash))
            fout.write(struct.pack('<I', len(materials)))
            for material_slot_name in materials.keys():
                material_slot_name_hash = xxhash.xxh64(material_slot_name.encode(string_format)).intdigest()
                fout.write(struct.pack('<Q', material_slot_name_hash))
            for material_path in materials.values():
                material_path_hash = xxhash.xxh64(material_path.encode(string_format)).intdigest()
                fout.write(struct.pack('<Q', material_path_hash))

if __name__ == "__main__":
    args = sys.argv
    argc = len(args)

    for arg_index in range(1, argc):
        file_path = args[arg_index]
        convert(file_path)