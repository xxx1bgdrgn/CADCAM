# CADCAM

独立的 Windows 二维激光 CAM 项目，使用 C++17、Qt 6 Widgets 和 Open CASCADE Technology（OCCT）。[二维激光 CAM 软件设计说明](docs/二维激光CAM软件设计说明.md)

当前框架提供 OCCT 画布、示例工程、底图／零件／板材／排样视图切换和基础撤销重做。双击左右列表中的条目即可切换视图。工具栏的「在底图添加矩形」可用来体验撤销和重做。

## 构建

准备 CMake 3.24 或以上、Visual Studio 2022 的 C++ x64 工具链、Qt 6 Widgets 的 MSVC x64 包和 OCCT 的 x64 开发包。需要在本机终端提供 Qt 安装前缀、`OpenCASCADEConfig.cmake` 所在目录和 OCCT 运行库目录。

```powershell
$qtPrefix = 'C:/sdk/Qt/msvc2022_64'
$occtCmake = 'C:/sdk/occt/cmake'
$occtBin = 'C:/sdk/occt/bin'
cmake -S . -B build/desktop -G 'Visual Studio 17 2022' -A x64 "-DCMAKE_PREFIX_PATH=$qtPrefix" "-DOpenCASCADE_DIR=$occtCmake"
cmake --build build/desktop --config Release
ctest --test-dir build/desktop -C Release --output-on-failure

$env:PATH = "$qtPrefix/bin;$occtBin;$env:PATH"
& ./build/desktop/Release/CADCAM.exe
```

如 OCCT 的第三方运行库位于单独目录，也将这些目录加入运行环境的 `PATH`。在 Windows 桌面环境可用 `CADCAM.exe --smoke` 检查四种视图和一次撤销重做。

只构建工程模型和测试时，可加 `-DCADCAM_BUILD_DESKTOP=OFF`，使用独立的构建目录。

## 后续实现

设计说明列出了完整产品范围。DWG/DXF 导入、交互绘图工具、实际工艺与刀路计算、自动排样、`.camproj` 读写以及 G 代码后处理属于后续模块。
