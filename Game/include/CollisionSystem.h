#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <variant>
#include <vector>
#include <SimpleMath.h>
#include "GameObject.h"

// 碰撞层：Collider 只和 mask 中包含的层发生检测（任意一方感兴趣即检测）
namespace CollisionLayer
{
    constexpr uint32_t Static = 1u << 0;   // 场景中不动的物体
    constexpr uint32_t Player = 1u << 1;
    constexpr uint32_t Enemy  = 1u << 2;
    constexpr uint32_t Weapon = 1u << 3;
}

struct SphereShape
{
    DirectX::SimpleMath::Vector3 center;
    float radius = 0.5f;
};

// 竖直胶囊体：中轴从 bottom 到 bottom + (0, height, 0)，适合角色
struct CapsuleShape
{
    DirectX::SimpleMath::Vector3 bottom;
    float height = 1.0f;
    float radius = 0.3f;
};

// 轴对齐包围盒
struct BoxShape
{
    DirectX::SimpleMath::Vector3 center;
    DirectX::SimpleMath::Vector3 extents { 0.5f, 0.5f, 0.5f };   // 半边长
};

using Shape = std::variant<SphereShape, CapsuleShape, BoxShape>;

// 形状为世界空间，由拥有者在移动后自行更新
struct Collider
{
    ObjectHandle owner;
    Shape shape;
    uint32_t layer = 0;
    uint32_t mask = 0;
    bool trigger = false;           // 触发器：只报告重叠，接收方不应做物理推挤
};

struct Contact
{
    DirectX::SimpleMath::Vector3 normal;    // 把接收方推离对方的方向
    float depth = 0.0f;                     // 穿透深度
    bool trigger = false;
    const Collider* self = nullptr;
    const Collider* other = nullptr;
};

// 集中的碰撞检测：所有对象 Update 之后由 Scene 调用 Detect，再把结果回调给双方
class CollisionSystem
{
public:
    Collider* Add(const Collider& collider);
    void Remove(Collider* collider);
    void Clear();

    // 对每个相交的碰撞体对调用 onHit(a, b, a 的 contact)
    void Detect(const std::function<void(const Collider&, const Collider&, const Contact&)>& onHit) const;

    // a 与 b 相交时返回 true，contact.normal 把 a 推离 b
    static bool Intersect(const Shape& a, const Shape& b, Contact& contact);

private:
    std::vector<std::unique_ptr<Collider>> m_colliders;
};
