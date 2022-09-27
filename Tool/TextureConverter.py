import os
import sys, os.path, subprocess, shutil, glob
import AssetDevelopmentConfig, tempfile

WORK_EXT = [".TGA", ".BMP", ".PNG", ".DDS"]
TEXCONV_PATH = "%s\\texconv\\texconv.exe" % AssetDevelopmentConfig.TOOL_ROOT
DDS_EXT = ".dds"

def convert(file_paths):
    max_process = 64
    remaining_count = len(file_paths)
    loop_count = int(len(file_paths) / max_process) + 1

    with tempfile.TemporaryDirectory() as temp_directory_path:
        for target_index in range(0, loop_count):
            target_count = min(max_process, remaining_count)
            remaining_count -= target_count
            print("target count: %d" % target_count)
            proc_list = []
            for arg_index in range(remaining_count, remaining_count + target_count):
                texture_file_path = file_paths[arg_index]
                file_name = os.path.basename(texture_file_path)

                format = "BC7_UNORM_SRGB"
                if "_CubeMap" in file_name:
                    format = "BC6H_UF16"

                cmd = []
                cmd.append(TEXCONV_PATH)
                cmd.append("-f")
                cmd.append(format)
                cmd.append("%s" % (texture_file_path))
                cmd.append("-m")
                cmd.append("0")
                cmd.append("-y")
                cmd.append("-o")
                cmd.append(temp_directory_path)
                # cmd.append("-nogpu")
                print(cmd)

                proc = subprocess.Popen(cmd)
                proc_list.append(proc)

            for proc in proc_list:
                proc.wait()

        print("texture convert completed!")
        for arg_index in range(0, len(file_paths)):
            file_path = file_paths[arg_index]
            file_name = os.path.basename(file_path)
            file_name, file_ext = os.path.splitext(file_name)
            work_dir = os.path.dirname(file_path)
            output_dir = work_dir.replace(AssetDevelopmentConfig.WORK_ROOT, AssetDevelopmentConfig.RESOURCE_ROOT)
            output_name = file_name + DDS_EXT
            output_path = os.path.join(output_dir, output_name)

            texture_work_dds_path = os.path.join(temp_directory_path, file_name + DDS_EXT)
            # texture_work_dds_path = file_path.replace(file_ext, DDS_EXT)

            if not os.path.exists(output_dir):
                os.makedirs(output_dir)
                print("mkdir %s" % output_dir)

            copy_path = shutil.move(texture_work_dds_path, output_path)
            print("move to %s" % copy_path)

    # os.system("pause")

if __name__ == "__main__":
    if len(sys.argv) == 1:
        glob_dirs = "%s\\**\\*.*" % AssetDevelopmentConfig.WORK_ROOT
        target_paths = glob.glob(glob_dirs, recursive=True)
        convert_file_list = []
        for target_path in target_paths:
            file_name, file_ext = os.path.splitext(target_path)
            if file_ext in WORK_EXT:
                convert_file_list.append(target_path)
        convert(convert_file_list)

    args = sys.argv
    argc = len(args)
    convert(args[1:])
