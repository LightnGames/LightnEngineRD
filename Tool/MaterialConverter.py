import sys, yaml, os.path, subprocess, struct, xxhash, glob
import AssetDevelopmentConfig

MAT_EXT = ".mto"

PARAMETER_SIZE_TABLE = {'Float': 4, 'Float2': 8, 'Float3': 12, 'Float4': 16, 'Texture': 8}
PARAMETER_TYPE_TABLE = {'Float': 0, 'Float2': 1, 'Float3': 2, 'Float4': 3, 'uint': 4, 'Texture': 5}

def convert(file_path):
    with open(file_path, 'r') as yml:
        material_data = yaml.load(yml, Loader=yaml.SafeLoader)
        pipeline_set_path = material_data['PipelineSet']
        parameters = material_data['Parameters']

        file_name = os.path.basename(file_path)
        file_name, file_ext = os.path.splitext(file_name)
        work_dir = os.path.dirname(file_path)
        output_dir = work_dir.replace(AssetDevelopmentConfig.WORK_ROOT, AssetDevelopmentConfig.RESOURCE_ROOT)
        output_name = file_name + MAT_EXT
        output_path = os.path.join(output_dir, output_name)

        if not os.path.exists(output_dir):
            os.makedirs(output_dir)
            print("mkdir %s" % output_dir)

        string_format = 'utf-8'
        with open(output_path, mode='wb') as fout:
            pipeline_set_path_hash = xxhash.xxh64(pipeline_set_path.encode(string_format)).intdigest()
            fout.write(struct.pack('<Q', pipeline_set_path_hash))

            header_size_in_byte = 8
            material_parameter_size = 0
            for parameter_item in parameters.values():
                material_parameter_size += header_size_in_byte
                material_parameter_size += PARAMETER_SIZE_TABLE[parameter_item[0]]

            material_parameter_count = len(parameters)
            material_parameter_size = material_parameter_size
            fout.write(struct.pack('<I', material_parameter_count))
            fout.write(struct.pack('<I', material_parameter_size))

            for parameter_key, parameter_item in parameters.items():
                parameter_type = PARAMETER_TYPE_TABLE[parameter_item[0]]
                parameter_name_hash = xxhash.xxh32(parameter_key.encode(string_format)).intdigest()
                fout.write(struct.pack('<I', parameter_name_hash))
                fout.write(struct.pack('<B', parameter_type))
                if parameter_type == PARAMETER_TYPE_TABLE['Float']:
                    fout.write(struct.pack('<f', parameter_item[1]))
                if parameter_type == PARAMETER_TYPE_TABLE['Float2']:
                    fout.write(struct.pack('<ff', *parameter_item[1]))
                if parameter_type == PARAMETER_TYPE_TABLE['Float3']:
                    fout.write(struct.pack('<fff', *parameter_item[1]))
                if parameter_type == PARAMETER_TYPE_TABLE['Float4']:
                    fout.write(struct.pack('<ffff', *parameter_item[1]))
                if parameter_type == PARAMETER_TYPE_TABLE['Texture']:
                    texture_path_hash = xxhash.xxh64(parameter_item[1].encode(string_format)).intdigest()
                    fout.write(struct.pack('<Q', texture_path_hash))

        print("Output -> %s" % output_path)

if __name__ == "__main__":
    args = sys.argv
    argc = len(args)

    for arg_index in range(1, argc):
        file_path = args[arg_index]
        convert(file_path)