#include "Game.h"

#include <memory>
#include "Environment.h"
#include "GameEvents.h"
#include "Log.h"
#include "Player.h"
#include "ThirdPersonCamera.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

bool Game::Initialize(const InitArgs& args, const GameOptions& options)
{
    m_frameCB.Create(args.device);
    m_objectCB.Create(args.device);

    // 生成顺序就是更新 / 描画顺序：相机先处理鼠标转向，玩家再按新朝向移动
    m_scene.Spawn(std::make_unique<Environment>());

    auto camera = std::make_unique<ThirdPersonCamera>();
    camera->SetYaw(XMConvertToRadians(options.cameraYawDeg));
    m_camera = m_scene.Spawn(std::move(camera));

    auto player = std::make_unique<Player>();
    player->autoRun = options.autoRun;
    m_scene.Spawn(std::move(player));

    m_scene.Events().Subscribe<PlayerStateChangedEvent>(this, [](const PlayerStateChangedEvent& e) {
        Log("Player state: %s -> %s", e.from, e.to);
    });

    return m_scene.Initialize(args);
}

void Game::Update(const UpdateArgs& args)
{
    m_scene.Update(args);
}

void Game::Draw(ID3D11DeviceContext* context, float aspect)
{
    const auto* camera = m_scene.Get<ThirdPersonCamera>(m_camera);
    if (!camera)
        return;

    FrameConstants frame = {};
    XMStoreFloat4x4(&frame.view, XMMatrixTranspose(camera->View()));
    XMStoreFloat4x4(&frame.proj, XMMatrixTranspose(Matrix::CreatePerspectiveFieldOfView(XMConvertToRadians(60.0f), aspect, 0.05f, 500.0f)));
    Vector3 lightDir(0.4f, -1.0f, 0.5f);
    lightDir.Normalize();
    frame.lightDir = lightDir;
    frame.cameraPos = camera->Position();
    m_frameCB.SetData(context, frame);

    ID3D11Buffer* frameCB = m_frameCB.GetBuffer();
    context->VSSetConstantBuffers(0, 1, &frameCB);
    context->PSSetConstantBuffers(0, 1, &frameCB);

    m_scene.Draw({ context, m_objectCB });
}

void Game::Finalize()
{
    m_scene.Finalize();
    m_objectCB = {};
    m_frameCB = {};
}
