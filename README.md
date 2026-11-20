# CADCAM

独立的 Windows 激光 CAM 项目，使用 C++17、Qt 6 Widgets 和 Open CASCADE Technology（OCCT）。仓库当前运行的是二维板材框架，三维管材是下一阶段的开发方向。

- [二维板材激光 CAM 软件设计说明](docs/二维板材激光CAM软件设计说明.md)
- [三维管材激光 CAM 软件设计说明](docs/三维管材激光CAM软件设计说明.md)

当前框架提供 OCCT 画布、示例工程、底图／零件／板材／排样视图切换和基础撤销重做。双击左右列表中的条目即可切换视图。工具栏的「在底图添加矩形」可用来体验撤销和重做。

## 构建

准备 CMake 4.2 或以上、Visual Studio 2026 的 MSVC v145 x64 工具链、Qt 6 Widgets 的 MSVC x64 包和 OCCT 的 x64 开发包。本机已验证 Qt 6.8.3 的 MSVC 2022 x64 包和 OCCT 7.9.3 可以链接到 VS 2026 编译的程序。

| 用途 | 本地 SDK 目录 | 链接模块 |
| --- | --- | --- |
| UI | `third_party/ui/qt` | `Qt6::Widgets` |
| 几何 | `third_party/cad/occt` | `TKTopAlgo`、`TKGeomBase` |
| 渲染 | `third_party/cad/occt` | `TKOpenGl`、`TKV3d` |

把 SDK 的 `include`、`lib` 和 `bin` 放在对应目录下。几何和渲染共用一份 OCCT SDK。CMake 会查找以上目录，这两套本地 SDK 由 Git 忽略。SDK 安装在其他位置时，可以显式指定 `CMAKE_PREFIX_PATH` 和 `OpenCASCADE_DIR`。

```powershell
cmake -S . -B build/vs2026 -G 'Visual Studio 18 2026' -A x64 -T v145
cmake --build build/vs2026 --config Release
ctest --test-dir build/vs2026 -C Release --output-on-failure

$qtPrefix = Join-Path $PWD 'third_party/ui/qt'
$occtBin = Join-Path $PWD 'third_party/cad/occt/bin'
$env:PATH = "$qtPrefix/bin;$occtBin;$env:PATH"
& ./build/vs2026/Release/CADCAM.exe
```

用 Visual Studio 2026 打开生成的 `build/vs2026/ide/CADCAM.slnx`，其中只显示 `CADCAM` 桌面项目。测试通过上面的 CTest 命令运行。修改 CMake 构建配置后，重新执行配置命令。若旧的构建缓存还引用原来的 SDK 位置，在配置命令加上 `-U 'Qt6*' -U OpenCASCADE_DIR -U CMAKE_PREFIX_PATH` 后重新生成。

如 OCCT 的第三方运行库位于单独目录，也将这些目录加入运行环境的 `PATH`。在 Windows 桌面环境可用 `CADCAM.exe --smoke` 检查四种视图和一次撤销重做。

只构建工程模型和测试时，可加 `-DCADCAM_BUILD_DESKTOP=OFF`，使用独立的构建目录。

## 后续实现

二维板材设计说明记录了原方案的产品范围。三维管材的首个里程碑是导入真实 STEP 管件，确认管轴与切割轮廓，设置工艺，预览刀路并保存工程。

## 源码目录

Visual Studio 中的分组与 `src/` 下的实际目录一致。

| 目录 | 职责 |
| --- | --- |
| `App` | 程序启动入口 |
| `UI` | 主窗口、工具栏和列表交互 |
| `Features` | 工程数据、当前视图和编辑命令 |
| `Geometry` | 基础图元及其 OCCT 几何构造 |
| `Rendering` | OCCT 画布及图形显示 |
| `Input/Mouse` | 画布的鼠标按下、拖动平移和滚轮缩放 |
| `Common` | 跨模块使用的通用工具 |
