import sys, yaml, os.path, subprocess, struct, xxhash, glob
import AssetDevelopmentConfig

WORK_EXT  = ".meshi"
OUTPUT_EXT = ".mesh"

def convert(file_path):
    with open(file_path, 'r') as yml:
        static_mesh_data = yaml.load(yml, Loader=yaml.SafeLoader)
        mesh_path = static_mesh_data['Mesh']

        materials = dict()
        if 'Material' in static_mesh_data:
            materials = static_mesh_data['Material']

        file_name = os.path.basename(file_path)
        file_name, file_ext = os.path.splitext(file_name)
        work_dir = os.path.dirname(file_path)
        output_dir = work_dir.replace(AssetDevelopmentConfig.WORK_ROOT, AssetDevelopmentConfig.RESOURCE_ROOT)
        output_name = file_name + OUTPUT_EXT
        output_path = os.path.join(output_dir, output_name)

        print("Material")
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
    if len(sys.argv) == 1:
        glob_dirs = "%s\\**\\*.*" % AssetDevelopmentConfig.WORK_ROOT
        target_paths = glob.glob(glob_dirs, recursive=True)
        for target_path in target_paths:
            file_name, ext = os.path.splitext(target_path)
            if ext == WORK_EXT:
                convert(target_path)

    args = sys.argv
    argc = len(args)

    for arg_index in range(1, argc):
        file_path = args[arg_index]
        convert(file_path)