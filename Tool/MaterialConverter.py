import sys, yaml, os.path, subprocess, struct, xxhash, glob
import AssetDevelopmentConfig, RruntimeMessageSender

MAT_EXT = ".mto"

def convert(file_path):
    with open(file_path, 'r') as yml:
        material_data = yaml.load(yml, Loader=yaml.SafeLoader)
        vertex_shader_path = material_data['VertexShader']
        pixel_shader_path = material_data['PixelShader']

        file_name = os.path.basename(file_path)
        file_name, file_ext = os.path.splitext(file_name)
        work_dir = os.path.dirname(file_path)
        output_dir = work_dir.replace(AssetDevelopmentConfig.WORK_ROOT, AssetDevelopmentConfig.RESOURCE_ROOT)
        output_name = file_name + MAT_EXT
        output_path = os.path.join(output_dir, output_name)

        if not os.path.exists(output_dir):
            os.makedirs(output_dir)
            print("mkdir %s" % output_dir)

        print("Output -> %s" % output_path)

        string_format = 'utf-8'
        with open(output_path, mode='wb') as fout:
            vertex_path_hash = xxhash.xxh64(vertex_shader_path.encode(string_format)).intdigest()
            pixel_path_hash = xxhash.xxh64(pixel_shader_path.encode(string_format)).intdigest()
            fout.write(struct.pack('<Q', vertex_path_hash))
            fout.write(struct.pack('<Q', pixel_path_hash))

if __name__ == "__main__":
    args = sys.argv
    argc = len(args)

    for arg_index in range(1, argc):
        file_path = args[arg_index]
        convert(file_path)