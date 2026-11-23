# CADCAM

Windows 激光 CAM 工作台，使用 C++17、Qt 6 Widgets 和 Open CASCADE Technology（OCCT）。二维板材与三维管材各有自己的应用、构建目录和 Visual Studio 解决方案。

- [二维板材激光 CAM 软件设计说明](docs/二维板材激光CAM软件设计说明.md)
- [三维管材激光 CAM 软件设计说明](docs/三维管材激光CAM软件设计说明.md)

| 目录 | 内容 |
| --- | --- |
| `CADCAM2D` | 二维画布、图元选择与框选、底图／零件／板材／排样视图和撤销重做。 |
| `CADCAM3D` | 文件入口和最近 STEP、新建管件向导、九种标准截面建模、管端编号和视角立方体。按住滚轮拖动旋转，右键拖动平移，滚动滚轮缩放。 |
| `common/Shape` | 两边共用的 `ShapeRecord`、图元类型、图形群组和工艺数据。各项目提供各自的二维或三维坐标与曲线参数。 |
| `third_party/common` | 两边共用的 Qt 和 OCCT SDK。 |

## 构建

准备 CMake 4.2 或以上、Visual Studio 2026 的 MSVC v145 x64 工具链、Qt 6 Widgets 的 MSVC x64 包和 OCCT 的 x64 开发包。本机已验证 Qt 6.8.3 的 MSVC 2022 x64 包和 OCCT 7.9.3 可以链接到 VS 2026 编译的程序。

| 用途 | 本地 SDK 目录 | 链接模块 |
| --- | --- | --- |
| UI | `third_party/common/ui/qt` | `Qt6::Widgets` |
| CAD 和渲染 | `third_party/common/cad/occt` | `TKTopAlgo`、`TKGeomBase`、`TKOpenGl`、`TKV3d` |
| 三维拉伸与 STEP | 同一份 OCCT SDK | `TKPrim`、`TKDESTEP`、`TKXCAF`，由三维项目链接 |

把 SDK 的 `include`、`lib`、`bin` 等目录放在对应位置。后续只给一个项目用的 SDK 放在 `CADCAM2D/third_party` 或 `CADCAM3D/third_party`，由该项目的 CMake 配置。SDK 和本机生成的构建文件由 Git 忽略。

在仓库根目录运行下面的命令生成解决方案。两个 `.sln` 各包含一个应用项目，打开后可直接按 F5。修改 CMake 或换机器时重新执行配置脚本。

```powershell
./Configure-VS.ps1
./Configure-VS.ps1 -Project CADCAM3D # 只生成三维项目
```

- 二维 `CADCAM2D/CADCAM2D.sln`
- 三维 `CADCAM3D/CADCAM3D.sln`

命令行构建和测试可使用 VS 2026 的开发者 PowerShell。

```powershell
cmake --build CADCAM2D/build/vs2026 --config Release
ctest --test-dir CADCAM2D/build/vs2026 -C Release --output-on-failure
cmake --build CADCAM3D/build/vs2026 --config Release
```

两个 EXE 都支持 `--smoke`。二维验证视图切换与撤销重做，三维验证直角及圆角管体、STEP 写入后重新导入和视图切换。从命令行直接运行时，将共用 SDK 的 `bin` 目录加到 `PATH`。

二维项目可通过 `-DCADCAM_BUILD_DESKTOP=OFF` 单独构建工程模型和测试。

## 后续实现

三维支持方矩管、圆管、椭圆管、腰形管、角钢、槽钢、工字钢、T 型钢和 H 型钢的参数创建，以及单实体 STEP 导入。工字钢和 H 型钢采用可输入腹板、翼缘尺寸的直边截面。下一阶段接入导入件的管轴与切割轮廓识别、异型管手绘截面、管材工艺和工程保存。

## 源码目录

Visual Studio 中的分组与每个项目 `src/` 下的实际目录一致。`Shared` 分组显示共用的 Shape 头文件。

| 目录 | 职责 |
| --- | --- |
| `App` | 程序启动入口 |
| `UI` | 主窗口、工具栏和列表交互 |
| `Features` | 工程数据、当前视图和编辑命令 |
| `Geometry` | 基础图元及其 OCCT 几何构造 |
| `Rendering` | OCCT 画布及图形显示 |
| `Input/Mouse` | 画布的鼠标按下、拖动平移和滚轮缩放 |
| `Common` | 跨模块使用的通用工具 |
