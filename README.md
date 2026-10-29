# SekiroDog

基于 **C++20 + Direct3D 11** 的第三人称 3D 动作游戏（开发中）。

## 当前进度

- 玩家角色：狼武士（Dog Warrior），GPU 骨骼蒙皮，Idle / Run 动画平滑过渡
- 操作：
  - `W/A/S/D`：相对相机方向奔跑，松开回到待机
  - 鼠标：点击窗口锁定鼠标后转动视角；`Esc` 解锁，再按一次退出
- 场景：棋盘格地面（1 米一格）+ 参照柱子

## 第三方库（git submodule，位于 `ThirdParty/`）

| 库 | 版本 | 用途 |
| --- | --- | --- |
| [DirectXTK](https://github.com/microsoft/DirectXTK) | may2026 | 键鼠输入、SimpleMath、WIC 贴图加载、截图、几何体 |
| [Assimp](https://github.com/assimp/assimp) | v6.0.5 | 读取 FBX（网格、骨骼、动画），只编译 FBX 导入器 |

克隆后先拉取子模块：

```bash
git submodule update --init --recursive
```

## 编译运行

需要 Visual Studio 2026（含“使用 C++ 的桌面开发”工作负载）。

- **Visual Studio 解决方案（推荐）**：双击 `generate_vs.cmd`，生成 `build/vs2026/SekiroDog.slnx`，用 VS 打开后按 F5 即可运行（`generate_vs.cmd open` 生成后直接打开）。
  - `SekiroDog` 已设为启动项目，调试工作目录为工程根目录；第三方库在解决方案的 `ThirdParty` 文件夹中
  - 修改了 CMakeLists.txt 或新增源文件后，VS 编译时会自动重新生成工程（ZERO_CHECK）
- 命令行：运行 `build.cmd`（Ninja，默认 Release，`build.cmd x64-debug` 编译 Debug），生成 `build/x64-release/Game/SekiroDog.exe`
- 也可以在 VS 中“打开文件夹”，使用 `CMakePresets.json` 中的 `x64-release` / `x64-debug` 预设

程序直接从工程目录读取 `Assets/` 和 `Game/shaders/`（路径在编译时写入），着色器在启动时编译。日志写到工作目录下的 `SekiroDog.log`。

调试参数：`--auto-run`（模拟按住 W）、`--camera-yaw <度>`、`--capture <png> <帧号>`（截图后退出）。

## 目录结构

```
Game/
  src/
    main.cpp               窗口、主循环、输入
    Graphics.*             D3D11 设备 / 交换链 / 着色器编译
    SkinnedModel.*         FBX 加载、动画采样与混合、蒙皮绘制
    Player.*               移动与动画状态（IsRunning 混合权重）
    ThirdPersonCamera.*    第三人称环绕相机
    Environment.*          地面与参照物
  shaders/                 Common.hlsli / Skinned.hlsl / Lit.hlsl
Assets/Player/             角色 FBX 与贴图（由 SourceArt 导出）
SourceArt/                 角色 .blend 源文件与导出脚本
ThirdParty/                DirectXTK、Assimp
```

## 美术资源流程

修改 `.blend` 后重新导出（减面到约 5 万顶点、导出贴图、生成待机动画、按 NLA 轨道导出 Run/Idle）：

```bash
blender -b SourceArt/Dog_Warrior_Fast_Run_HandsFixed.blend --python SourceArt/export_player_fbx.py
```

- 源文件中没有完整的待机动画，`Idle` 由脚本里的 `make_idle_action()` 生成（手臂下垂 + 呼吸起伏）
- 新增动画：在 `CLIPS` 中加入 `"片段名": "Blender动作名"`，代码中用 `SkinnedModel::FindClip("片段名")` 取用

> `.blend` 源文件约 104MB，超过 GitHub 单文件 100MB 限制，提交前请使用 Git LFS 或不纳入版本库。

## 素材声明

角色模型、动作、贴图等演示资源仅用于学习与个人作品展示，不用于商业用途。
