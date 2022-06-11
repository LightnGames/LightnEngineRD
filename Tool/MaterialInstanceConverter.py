import sys, yaml, os.path, subprocess, struct, xxhash, glob
import AssetDevelopmentConfig

MATI_EXT = ".mti"

def convert(file_path):
    with open(file_path, 'r') as yml:
        material_instance_data = yaml.load(yml, Loader=yaml.SafeLoader)
        material_path = material_instance_data['Material']

        file_name = os.path.basename(file_path)
        file_name, file_ext = os.path.splitext(file_name)
        work_dir = os.path.dirname(file_path)
        output_dir = work_dir.replace(AssetDevelopmentConfig.WORK_ROOT, AssetDevelopmentConfig.RESOURCE_ROOT)
        output_name = file_name + MATI_EXT
        output_path = os.path.join(output_dir, output_name)

        if not os.path.exists(output_dir):
            os.makedirs(output_dir)
            print("mkdir %s" % output_dir)

        print("Output -> %s" % output_path)

        # Shader Param Type
        # 0: float
        # 1: float2
        # 2: float3
        # 3: float4
        # 4: uint
        # 5: texture

        string_format = 'utf-8'
        with open(output_path, mode='wb') as fout:
            material_path_hash = xxhash.xxh64(material_path.encode(string_format)).intdigest()
            fout.write(struct.pack('<Q', material_path_hash))

            material_parameter_count = 2
            material_parameter_size = 34
            fout.write(struct.pack('<I', material_parameter_count))
            fout.write(struct.pack('<I', material_parameter_size))

            texture_path_hash = xxhash.xxh32("BaseColor".encode(string_format)).intdigest()
            fout.write(struct.pack('<I', texture_path_hash))
            fout.write(struct.pack('<B', 3))
            fout.write(struct.pack('<ffff', *[1.0, 1.0, 1.0, 1.0]))

            texture_path_hash = xxhash.xxh32("BaseColorTexture".encode(string_format)).intdigest()
            fout.write(struct.pack('<I', texture_path_hash))
            fout.write(struct.pack('<B', 5))
            fout.write(struct.pack('<Q', 0))

if __name__ == "__main__":
    args = sys.argv
    argc = len(args)

    for arg_index in range(1, argc):
        file_path = args[arg_index]
        convert(file_path)