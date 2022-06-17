import sys, yaml, os.path, subprocess, shutil
import AssetDevelopmentConfig

TEXCONV_PATH = "%s\\texconv\\texconv.exe" % AssetDevelopmentConfig.TOOL_ROOT
DDS_EXT = ".dds"

if __name__ == "__main__":
    max_process = 64
    args = sys.argv
    argc = len(args)
    remaining_count = argc - 1
    loop_count = int(argc / max_process) + 1

    for target_index in range(0, loop_count):
        target_count = min(max_process, remaining_count)
        remaining_count -= target_count
        print("target count: %d" % target_count)
        proc_list = []
        for arg_index in range(remaining_count, remaining_count + target_count):
            texture_file_path = args[arg_index + 1]

            cmd = []
            cmd.append(TEXCONV_PATH)
            cmd.append("-f")
            cmd.append("BC7_UNORM_SRGB")
            cmd.append("%s" % (texture_file_path.replace('\\', '/')))
            cmd.append("-m")
            cmd.append("0")
            cmd.append("-y")
            # cmd.append("-nogpu")
            print(cmd)

            proc = subprocess.Popen(cmd)
            proc_list.append(proc)

        for proc in proc_list:
            proc.wait()

    print("texture convert completed!")
    for arg_index in range(1, argc):
        file_path = args[arg_index]
        file_name = os.path.basename(file_path)
        file_name, file_ext = os.path.splitext(file_name)
        work_dir = os.path.dirname(file_path)
        output_dir = work_dir.replace(AssetDevelopmentConfig.WORK_ROOT, AssetDevelopmentConfig.RESOURCE_ROOT)
        output_name = file_name + DDS_EXT
        output_path = os.path.join(output_dir, output_name)

        texture_work_dds_path = texture_file_path.replace(file_ext, DDS_EXT)

        if not os.path.exists(output_dir):
            os.makedirs(output_dir)
            print("mkdir %s" % output_dir)

        copy_path = shutil.move(texture_work_dds_path, output_path)
        print("move to %s" % copy_path)

    # os.system("pause")

