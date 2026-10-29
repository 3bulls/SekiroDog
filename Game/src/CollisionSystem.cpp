#include "CollisionSystem.h"

#include <algorithm>
#include <cmath>

using namespace DirectX::SimpleMath;

namespace
{
    constexpr float Epsilon = 1e-6f;

    bool SphereSphere(const Vector3& ca, float ra, const Vector3& cb, float rb, Contact& c)
    {
        const Vector3 d = ca - cb;
        const float dist2 = d.LengthSquared();
        const float r = ra + rb;
        if (dist2 >= r * r)
            return false;
        const float dist = std::sqrt(dist2);
        c.normal = dist > Epsilon ? d / dist : Vector3::Up;
        c.depth = r - dist;
        return true;
    }

    bool SphereBox(const Vector3& center, float radius, const BoxShape& box, Contact& c)
    {
        const Vector3 lo = box.center - box.extents;
        const Vector3 hi = box.center + box.extents;
        Vector3 closest;
        center.Clamp(lo, hi, closest);
        const Vector3 d = center - closest;
        const float dist2 = d.LengthSquared();

        if (dist2 > Epsilon)
        {
            if (dist2 >= radius * radius)
                return false;
            const float dist = std::sqrt(dist2);
            c.normal = d / dist;
            c.depth = radius - dist;
            return true;
        }

        // 球心在盒子内部：沿穿透最浅的轴推出
        const Vector3 local = center - box.center;
        const float px = box.extents.x - std::abs(local.x);
        const float py = box.extents.y - std::abs(local.y);
        const float pz = box.extents.z - std::abs(local.z);
        if (px <= py && px <= pz)
        {
            c.normal = Vector3(local.x >= 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f);
            c.depth = px + radius;
        }
        else if (py <= pz)
        {
            c.normal = Vector3(0.0f, local.y >= 0.0f ? 1.0f : -1.0f, 0.0f);
            c.depth = py + radius;
        }
        else
        {
            c.normal = Vector3(0.0f, 0.0f, local.z >= 0.0f ? 1.0f : -1.0f);
            c.depth = pz + radius;
        }
        return true;
    }

    bool BoxBox(const BoxShape& a, const BoxShape& b, Contact& c)
    {
        const Vector3 d = a.center - b.center;
        const Vector3 overlap = a.extents + b.extents - Vector3(std::abs(d.x), std::abs(d.y), std::abs(d.z));
        if (overlap.x <= 0.0f || overlap.y <= 0.0f || overlap.z <= 0.0f)
            return false;
        if (overlap.x <= overlap.y && overlap.x <= overlap.z)
        {
            c.normal = Vector3(d.x >= 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f);
            c.depth = overlap.x;
        }
        else if (overlap.y <= overlap.z)
        {
            c.normal = Vector3(0.0f, d.y >= 0.0f ? 1.0f : -1.0f, 0.0f);
            c.depth = overlap.y;
        }
        else
        {
            c.normal = Vector3(0.0f, 0.0f, d.z >= 0.0f ? 1.0f : -1.0f);
            c.depth = overlap.z;
        }
        return true;
    }

    // 竖直胶囊中轴上离高度 y 最近的点
    Vector3 PointOnAxis(const CapsuleShape& cap, float y)
    {
        return Vector3(cap.bottom.x, std::clamp(y, cap.bottom.y, cap.bottom.y + cap.height), cap.bottom.z);
    }

    // 胶囊体都化简为“中轴上最近点处的球”再检测
    struct Tester
    {
        Contact& c;

        bool operator()(const SphereShape& a, const SphereShape& b) const { return SphereSphere(a.center, a.radius, b.center, b.radius, c); }
        bool operator()(const SphereShape& a, const BoxShape& b) const { return SphereBox(a.center, a.radius, b, c); }
        bool operator()(const BoxShape& a, const BoxShape& b) const { return BoxBox(a, b, c); }

        bool operator()(const CapsuleShape& a, const SphereShape& b) const
        {
            return SphereSphere(PointOnAxis(a, b.center.y), a.radius, b.center, b.radius, c);
        }
        bool operator()(const CapsuleShape& a, const BoxShape& b) const
        {
            // 竖直线段与 AABB 的最近点：把盒子中心高度夹到线段范围内
            return SphereBox(PointOnAxis(a, b.center.y), a.radius, b, c);
        }
        bool operator()(const CapsuleShape& a, const CapsuleShape& b) const
        {
            // 两条竖直线段：高度有重叠时取重叠区中点，否则取最近的端点
            const float lo = std::max(a.bottom.y, b.bottom.y);
            const float hi = std::min(a.bottom.y + a.height, b.bottom.y + b.height);
            const float y = lo <= hi ? (lo + hi) * 0.5f : (a.bottom.y > b.bottom.y ? lo : hi);
            return SphereSphere(PointOnAxis(a, y), a.radius, PointOnAxis(b, y), b.radius, c);
        }

        // 其余组合交换参数，法线取反
        template<class A, class B>
        bool operator()(const A& a, const B& b) const
        {
            if (!(*this)(b, a))
                return false;
            c.normal = -c.normal;
            return true;
        }
    };
}

Collider* CollisionSystem::Add(const Collider& collider)
{
    m_colliders.push_back(std::make_unique<Collider>(collider));
    return m_colliders.back().get();
}

void CollisionSystem::Remove(Collider* collider)
{
    std::erase_if(m_colliders, [collider](const std::unique_ptr<Collider>& c) { return c.get() == collider; });
}

void CollisionSystem::Clear()
{
    m_colliders.clear();
}

bool CollisionSystem::Intersect(const Shape& a, const Shape& b, Contact& contact)
{
    return std::visit(Tester{ contact }, a, b);
}

void CollisionSystem::Detect(const std::function<void(const Collider&, const Collider&, const Contact&)>& onHit) const
{
    // 目前物体很少，直接两两检测；物体多了再换成空间划分（网格 / BVH）
    for (size_t i = 0; i < m_colliders.size(); ++i)
        for (size_t j = i + 1; j < m_colliders.size(); ++j)
        {
            const Collider& a = *m_colliders[i];
            const Collider& b = *m_colliders[j];
            if (a.owner == b.owner || (!(a.mask & b.layer) && !(b.mask & a.layer)))
                continue;

            Contact contact;
            if (!Intersect(a.shape, b.shape, contact))
                continue;
            contact.trigger = a.trigger || b.trigger;
            contact.self = &a;
            contact.other = &b;
            onHit(a, b, contact);
        }
}
