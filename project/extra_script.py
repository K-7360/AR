import os
from pathlib import Path

Import("env")

# 获取项目根目录
project_dir = env.subst("$PROJECT_DIR")

# 添加包含路径以使编译器能找到头文件
include_paths = [
    os.path.join(project_dir, "..", "Phase1", "ads1299"),
    os.path.join(project_dir, "..", "Phase1", "hal"),
    os.path.join(project_dir, "..", "Phase1", "test")
]

for path in include_paths:
    abs_path = os.path.abspath(path)
    if os.path.exists(abs_path):
        env.Append(CPPPATH=[abs_path])
        print(f"Added include path: {abs_path}")