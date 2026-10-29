# SekiroDog

基于 **C++20 + Direct3D 11** 的第三人称 3D 动作游戏（开发中）。

## 当前进度

- 玩家角色：狼武士（Dog Warrior），GPU 骨骼蒙皮，Idle / Run 动画平滑过渡
- 操作：
  - `W/A/S/D`：相对相机方向奔跑，松开回到待机
  - 鼠标：点击窗口锁定鼠标后转动视角；`Q` 或 `Esc` 解锁鼠标，未锁定时按 `Esc` 退出
- 窗口：画面 1920×1080，屏幕放不下时自动最大化
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
    Application.h          窗口、消息循环、D3D 设备、键鼠输入、帧计时
    Game.h                 游戏逻辑：搭建场景并驱动 Scene
    Scene.h                对象容器：句柄查找、延迟增删、分阶段更新
    GameObject.h           对象基类与生命周期、ObjectHandle
    CollisionSystem.h      碰撞体（球 / 竖直胶囊 / AABB）、层过滤、集中检测
    EventBus.h             事件总线（延迟分发）
    GameEvents.h           游戏事件定义
    StateMachine.h         通用有限状态机模板
    Player.h               玩家（状态机驱动）
    PlayerStates.h         玩家状态：Idle / Run
    Animator.h             动画播放与淡入淡出
    ThirdPersonCamera.h    第三人称环绕相机
    Environment.h          地面与参照柱子（静态碰撞体）
    Graphics.h             D3D11 设备 / 交换链 / 着色器编译
    SkinnedModel.h         FBX 加载、动画采样与混合、蒙皮绘制
    Log.h                  日志
  src/                     源文件（main.cpp 只创建 Application，其余为上述头文件的实现）
  shaders/                 Common.hlsli / Skinned.hlsl / Lit.hlsl
ThirdParty/                第三方库源码（见 ThirdParty/README.md）
  DirectXTK/               may2026：键鼠输入、SimpleMath、贴图加载、截图、几何体
  assimp/                  v6.0.5：读取 FBX（只编译 FBX 导入器）
Assets/Player/             角色 FBX 与贴图（由 SourceArt 导出）
SourceArt/                 角色导出脚本（.blend 源文件不纳入版本库）
bin/  obj/                 编译输出（不纳入版本库）
```

## 代码结构

### 分层

- `main.cpp`：初始化 COM 与 DPI，创建 `Application` 并 `Run()`
- `Application`：负责窗口相关的一切，包括命令行、窗口、消息循环、D3D 设备、键鼠输入（点击锁定鼠标，Q 或 Esc 解锁）、帧计时和截图
- `Game`：只管游戏逻辑。在 `Scene` 中生成对象，每帧驱动 `Scene`，并写入相机的帧常量
- `Scene`：拥有所有 `GameObject`，提供对象之间交互所需的机制（见下文）

### 每帧顺序（Scene::Update）

```
Update       各对象处理输入与意图（相机转向 → 玩家状态机）
Collision    CollisionSystem 统一检测，回调双方的 OnCollision
Events       分发本帧积压的事件
LateUpdate   依赖其他对象最终状态的逻辑（相机跟随玩家）
Commit       执行本帧的 Spawn / Destroy
Draw
```

对象按生成顺序更新与描画，Finalize 时按逆序释放。

### 对象之间的交互

| 类型 | 机制 | 例子 |
| --- | --- | --- |
| 查询：A 读取 B 的状态 | `Scene::Find<T>()` 查找，保存 `ObjectHandle`，用时 `Scene::Get<T>(handle)` | 相机跟随玩家；玩家读取相机朝向 |
| 空间：两个物体接触 | 在 `CollisionSystem` 注册 `Collider`（用 layer / mask 过滤），重写 `OnCollision` | 玩家撞到柱子后沿法线推出，可贴墙滑动 |
| 通知：A 发生了某件事 | `Events().Publish(事件)` / `Events().Subscribe<事件>(owner, 处理函数)`，帧内统一分发 | 玩家切换状态时 Game 写日志 |

- 句柄由“槽位下标 + 代数”组成。对象销毁后旧句柄自动失效，`Get` 返回 `nullptr`，不会出现野指针
- `Spawn` / `Destroy` 都延迟到帧末尾生效，在遍历对象的过程中增删是安全的。同一批生成的对象在 `Initialize` 中可以互相 `Find`
- 对象被销毁时，Scene 会自动退订它的事件

### 玩家状态机

- `StateMachine<Owner, Id>` 是通用模板。每个状态实现 `Enter / Update / Exit`
- 状态在 `Update` 中调用 `ChangeState` 请求切换，当前 `Update` 结束后才生效（旧状态 `Exit`，新状态 `Enter`）
- `Player` 只提供状态可以调用的动作：`MoveDirection`、`TurnTowards`、`Move`、`PlayAnimation`。具体行为写在 `PlayerStates.cpp`
- 新增状态（例如攻击、闪避）的步骤：
  1. 在 `PlayerStateId` 中加一个枚举值
  2. 在 `PlayerStates.h/.cpp` 中写一个状态类
  3. 在 `Player::Initialize` 中 `Add` 这个状态

### 新增对象

写一个继承 `GameObject` 的类，在 `Game::Initialize` 中用 `m_scene.Spawn(std::make_unique<T>())` 生成。

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
