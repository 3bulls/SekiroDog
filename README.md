# SekiroDog

基于 **C++20 + Direct3D 11** 的第三人称 3D 动作游戏（开发中）。

## 当前进度

- 玩家角色：狼武士（Dog Warrior），GPU 骨骼蒙皮，Idle / Run 动画平滑过渡
- 操作：
  - `W/A/S/D`：相对相机方向奔跑，松开回到待机
  - 鼠标：点击窗口锁定鼠标后转动视角；`Esc` 解锁，再按一次退出
- 场景：棋盘格地面（1 米一格）+ 参照柱子

## 编译运行

需要 Visual Studio 2026（含“使用 C++ 的桌面开发”工作负载）。

1. 双击根目录的 **`SekiroDog.sln`**
2. 选择 `Debug` 或 `Release`（x64），按 **F5** 运行

`SekiroDog` 是默认启动项目，第三方库会随解决方案一起从源码编译。命令行编译：`build.cmd`（Release）或 `build.cmd Debug`，输出在 `bin/x64/<配置>/SekiroDog.exe`。

程序启动时从 exe 所在目录向上查找工程根目录，读取 `Assets/` 和 `Game/shaders/`；着色器在启动时编译，修改后重启即可生效。日志写到工作目录下的 `SekiroDog.log`。

调试参数：`--auto-run`（模拟按住 W）、`--camera-yaw <度>`、`--capture <png> <帧号>`（截图后退出）。

## 目录结构

```
SekiroDog.sln              解决方案（直接用 VS 打开）
Game/
  SekiroDog.vcxproj        游戏工程
  include/                 头文件
    Graphics.h             D3D11 设备 / 交换链 / 着色器编译
    SkinnedModel.h         FBX 加载、动画采样与混合、蒙皮绘制
    Player.h               移动与动画状态
    ThirdPersonCamera.h    第三人称环绕相机
    Environment.h          地面与参照物
    Log.h                  日志
  src/                     源文件（main.cpp 及上述头文件的实现）
  shaders/                 Common.hlsli / Skinned.hlsl / Lit.hlsl
ThirdParty/                第三方库源码（见 ThirdParty/README.md）
  DirectXTK/               may2026：键鼠输入、SimpleMath、贴图加载、截图、几何体
  assimp/                  v6.0.5：读取 FBX（只编译 FBX 导入器）
Assets/Player/             角色 FBX 与贴图（由 SourceArt 导出）
SourceArt/                 角色导出脚本（.blend 源文件不纳入版本库）
bin/  obj/                 编译输出（不纳入版本库）
```

## 美术资源流程

修改 `.blend` 后重新导出（减面到约 5 万顶点、导出贴图、生成待机动画、按 NLA 轨道导出 Run/Idle）：

```bash
blender -b SourceArt/Dog_Warrior_Fast_Run_HandsFixed.blend --python SourceArt/export_player_fbx.py
```

- 源文件中没有完整的待机动画，`Idle` 由脚本里的 `make_idle_action()` 生成（手臂下垂 + 呼吸起伏）
- 新增动画：在 `CLIPS` 中加入 `"片段名": "Blender动作名"`，代码中用 `SkinnedModel::FindClip("片段名")` 取用

> `.blend` 源文件约 104MB，超过 GitHub 单文件 100MB 限制，已在 .gitignore 中排除，请自行备份。

## 素材声明

角色模型、动作、贴图等演示资源仅用于学习与个人作品展示，不用于商业用途。
