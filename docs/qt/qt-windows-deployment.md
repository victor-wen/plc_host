# Qt 6.8 Windows 部署

> Qt 版本：6.8（官方页面当前显示的补丁版本为 6.8.8）
> 拉取日期：2026-08-06
> 来源：<https://doc.qt.io/qt-6.8/windows-deployment.html>、<https://doc.qt.io/qt-6.8/cmake-deployment.html>、<https://doc.qt.io/qt-6.8/qt-generate-deploy-app-script.html>、<https://doc.qt.io/qt-6.8/cmake-command-reference.html>、<https://doc.qt.io/qt-6.8/threads-qobject.html>
> Context7 libraryId：`/websites/doc_qt_io_qt-6`

## 部署目标

Windows 发布包必须能在没有 Qt 开发环境、没有 Qt 环境变量的干净 Windows 10/11 x64 机器上启动。Qt 官方的两条路径是：

1. 用 `windeployqt` 扫描已构建的 `.exe`，复制 Qt DLL、插件、翻译和（默认）编译器运行时。
2. 用 CMake 的 `install(TARGETS)` 安装程序，再用 `qt_generate_deploy_app_script()` 在安装阶段生成自包含目录。

## windeployqt

工具位于 `<QTDIR>\bin\windeployqt.exe`，应在对应 Qt/MSVC 构建环境中执行；Qt Online Installer 安装可先运行 `<QTDIR>\bin\qtenv2.bat`。

基本格式：

```text
windeployqt [options] [files]
```

发布构建示例：

```powershell
& "$env:QT_ROOT\bin\windeployqt.exe" `
    --release `
    --dir "$pwd\stage" `
    --compiler-runtime `
    --no-translations `
    "$pwd\build\plc_host.exe"
```

`files` 可以是 `.exe`，也可以是包含 `.exe` 的目录。工具扫描可执行文件的依赖，并把识别出的 Qt 库和插件复制到目标目录；使用 `--qmldir` 时还会通过 `qmlimportscanner` 扫描 QML import。Qt Widgets 项目通常不需要 `--qmldir`。

### 常用选项

| 选项 | 用途 |
|---|---|
| `--release` / `--debug` | 指明扫描的是 release/debug 二进制 |
| `--dir <directory>` | 把部署树放到指定目录，而不是 exe 所在目录 |
| `--dry-run` | 只模拟，不复制或更新文件 |
| `--list relative` | 输出将复制的相对路径 |
| `--list mapping` | 输出源文件到目标相对路径映射 |
| `--json` | 以 JSON 输出扫描结果 |
| `--verbose <0-2>` | 调高诊断日志级别 |
| `--no-compiler-runtime` | 不复制编译器运行时 |
| `--compiler-runtime` | 显式请求复制编译器运行时 |
| `--no-plugins` / `--no-libraries` | 调试时禁止复制插件/库 |
| `--no-translations` / `--translations de,zh` | 控制 Qt 翻译文件 |
| `--pdb` | 部署 MSVC `.pdb` 文件 |
| `--no-opengl-sw` | 不部署软件 OpenGL 光栅化库 |

Qt 库可按工具 `--help` 输出的模块名显式包含/排除，例如 `-serialbus`、`-sql`、`-widgets`；正常发布优先让依赖扫描决定。Qt Charts 的 `Qt6Charts.dll` 应通过 exe 的直接依赖被扫描到，不要假定每个 windeployqt 版本都列出相同的模块开关。

项目使用 Qt SerialBus、Qt SQL、Qt Charts、Qt Widgets 时，至少检查：

```text
Qt6Core.dll
Qt6Gui.dll
Qt6Widgets.dll
Qt6SerialBus.dll
Qt6Sql.dll
Qt6Charts.dll
platforms\qwindows.dll
sqldrivers\qsqlite.dll
```

是否需要 `imageformats`、`styles`、`printsupport`、`network` 等子目录取决于程序实际使用的功能。`windeployqt` 不会替应用识别所有第三方库；Qt 文档明确提醒数据库客户端等额外依赖要单独部署。

## CMake install + qt_generate_deploy_app_script

Qt 6.8 官方 CMake 部署 API 要求先安装 target，再安装部署脚本：

```cmake
cmake_minimum_required(VERSION 3.24)
project(plc_host LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets SerialBus Sql Charts)
qt_standard_project_setup()

qt_add_executable(plc_host
    src/app/main.cpp
)
target_link_libraries(plc_host PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::SerialBus
    Qt6::Sql
    Qt6::Charts
)

install(TARGETS plc_host
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

qt_generate_deploy_app_script(
    TARGET plc_host
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
```

`qt_generate_deploy_app_script()` 定义在 Qt6 Core CMake API 中，Qt 6.3 起提供；它在 CMake 生成阶段写入脚本，脚本在 `install()` 阶段调用 `qt_deploy_runtime_dependencies()`。Windows 桌面默认也安装编译器运行时；需要自行提供 VCRedist 时可传 `NO_COMPILER_RUNTIME`。`DEPLOY_TOOL_OPTIONS` 可转发给底层 `windeployqt`，例如：

```cmake
qt_generate_deploy_app_script(
    TARGET plc_host
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
    DEPLOY_TOOL_OPTIONS --no-translations
)
```

执行：

```powershell
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="$env:QT_ROOT"
cmake --build build --config Release
cmake --install build --config Release --prefix "$pwd\stage"
```

`qt_standard_project_setup()` 会引入 `GNUInstallDirs`，因此 `CMAKE_INSTALL_BINDIR` 可用于 Windows runtime 目标目录。脚本适用于在 Windows 主机上构建 Windows executable；不支持在 Linux 主机交叉构建 Windows executable 时直接使用该部署脚本，除非按官方选项显式关闭不支持平台错误，但仍需自行验证。

## MSVC 运行时和常见缺失 DLL

### MSVC 运行时

release 包必须处理 MSVC C/C++ runtime。选择其一：

- 让 `windeployqt --compiler-runtime` 或 CMake 部署脚本复制运行时 DLL；
- 在安装器中安装匹配架构的 `vc_redist.x64.exe`。

官方部署文档的示例包含 `vcruntime140.dll`、`msvcp140.dll`（不同 VS 17 工具链/文档示例可能显示带版本后缀的名称）。不要手工凭文件名猜版本；用依赖检查工具确认目标 exe 和 Qt DLL 实际需要的文件，并保证编译器版本、架构和运行时一致。

### 缺失文件诊断表

| 现象 | 优先检查 |
|---|---|
| 应用启动即报平台插件错误 | `platforms\qwindows.dll` 和 Qt6Gui/Qt6Core 版本是否匹配 |
| SQLite 驱动不可用 | `sqldrivers\qsqlite.dll` 是否存在，Qt SQL 插件是否与 Qt6Sql 同版本 |
| Charts/SerialBus 加载失败 | `Qt6Charts.dll`、`Qt6SerialBus.dll` 及其依赖是否被扫描 |
| 图片、打印或样式缺失 | `imageformats`、`printsupport`、`styles` 子目录及对应插件 |
| MSVC 启动错误 | VCRedist/`vcruntime`/`msvcp` 与 x64 release 构建是否一致 |
| OpenGL 初始化失败 | `opengl32sw.dll`（若启用软件 OpenGL）及显卡驱动 |
| ICU/OpenSSL/数据库第三方错误 | Qt 构建选项和应用直接依赖的第三方 DLL；windeployqt 不负责所有第三方库 |

所有 Qt 插件必须放在对应插件类型子目录；例如 Windows 平台插件放 `platforms`，SQLite 驱动放 `sqldrivers`。对于非 relocatable Qt 构建，还要用 `qt.conf` 或 `QCoreApplication::addLibraryPath()`/`setLibraryPaths()` 确保插件搜索路径正确。

## 依赖检查和干净机验收

构建机上先做不修改文件的扫描：

```powershell
windeployqt --release --dry-run --list relative build\Release\plc_host.exe
windeployqt --release --dry-run --json build\Release\plc_host.exe
```

再使用 Windows SDK/MSVC 的 `dumpbin` 检查直接依赖：

```powershell
dumpbin /DEPENDENTS stage\plc_host.exe
dumpbin /DEPENDENTS stage\Qt6Sql.dll
```

Qt 官方部署页也建议使用 Dependency Walker 查看应用依赖。最后把 `stage` 复制到没有 Qt、没有 Qt 环境变量、没有开发工具的干净 Windows x64 机器，验证：启动、SQLite 打开、Modbus 模块加载、Charts 显示、平台插件加载和退出流程。不要只在开发机上运行，因为开发机的 `PATH` 可能掩盖缺失 DLL。

## 线程安全约束

部署命令是构建/安装阶段的外部工具，不应在应用运行时由 UI 线程动态修改 Qt DLL 或插件目录。部署后的运行时规则不变：`QWidget`、Graphics View、Charts 在 UI 主线程；通信和数据库 QObject 在各自线程；跨线程仅使用信号/槽或 `QMetaObject::invokeMethod()`，不得直接访问另一线程对象。
