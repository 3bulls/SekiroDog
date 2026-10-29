# 第三方库（源码集成）

两个库都以源码形式放在这里，随 `SekiroDog.sln` 一起编译成静态库，不需要额外下载。

| 库 | 版本 | 来源 | 工程文件 |
| --- | --- | --- | --- |
| DirectXTK | `may2026`（5a8f5d0） | https://github.com/microsoft/DirectXTK | `DirectXTK/DirectXTK_Desktop_2026.vcxproj`（官方原版） |
| Assimp | `v6.0.5`（392a658） | https://github.com/assimp/assimp | `assimp/assimp.vcxproj`（自建，见下文） |

## DirectXTK

保留了 `Inc/`、`Src/`、`Audio/`、官方 VS 2026 工程和 LICENSE。
首次编译时工程会自动调用 `Src/Shaders/CompileShaders.cmd` 生成 `Src/Shaders/Compiled/*.inc`（已加入 .gitignore）。

## Assimp

- 保留 `include/`、`code/`（全部导入器源码）、`contrib/` 中用到的部分（zlib、unzip、pugixml、utf8cpp、rapidjson、openddlparser、Open3DGC、clipper、poly2tri、stb、earcut-hpp），删除了测试模型、googletest、draco 等
- `include/assimp/config.h`、`include/assimp/revision.h`、`contrib/zlib/zconf.h` 原本由 CMake 生成，这里直接提交
- `assimp.vcxproj` 只编译 **FBX 导入器**（其他导入器通过 `ASSIMP_BUILD_NO_*_IMPORTER` 关闭，不编译导出器），zlib 一并编进 `assimp.lib`
- 需要其他格式（如 glTF、OBJ）时：在 `assimp.vcxproj` 的预处理器定义中删掉对应的 `ASSIMP_BUILD_NO_XXX_IMPORTER`，并把 `code/AssetLib/XXX/` 下的 .cpp 加入工程
