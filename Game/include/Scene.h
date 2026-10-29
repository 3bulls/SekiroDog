#pragma once
#include <memory>
#include <utility>
#include <vector>
#include "CollisionSystem.h"
#include "EventBus.h"
#include "GameObject.h"

// 场景：拥有所有 GameObject，提供对象查找、碰撞系统和事件总线，并按阶段驱动每一帧。
// Spawn / Destroy 都是延迟的，在帧末尾统一生效，遍历对象时增删不会出问题。
class Scene
{
public:
    bool Initialize(const InitArgs& args);
    void Update(const UpdateArgs& args);    // Update → 碰撞 → 事件 → LateUpdate → 增删对象
    void Draw(const DrawArgs& args);
    void Finalize();

    // 对象在下一次 Commit 时加入场景并 Initialize（同一批生成的对象在 Initialize 里可以互相查找）
    ObjectHandle Spawn(std::unique_ptr<GameObject> object);
    void Destroy(ObjectHandle handle);      // 在帧末尾 Finalize 并移除
    bool Commit();                          // 立即处理待增删的对象；Initialize 失败时返回 false

    GameObject* Get(ObjectHandle handle) const;
    template<class T> T* Get(ObjectHandle handle) const { return dynamic_cast<T*>(Get(handle)); }
    template<class T> T* Find() const
    {
        for (const Slot& slot : m_slots)
            if (auto* object = dynamic_cast<T*>(slot.object.get()))
                return object;
        return nullptr;
    }

    CollisionSystem& Collisions() { return m_collisions; }
    EventBus& Events() { return m_events; }

private:
    struct Slot
    {
        std::unique_ptr<GameObject> object;
        uint32_t generation = 0;
    };

    // 按下标遍历：回调里 Spawn 会让 m_slots 扩容，范围 for 的迭代器会失效
    template<class F> void ForEach(F&& f)
    {
        for (size_t i = 0; i < m_slots.size(); ++i)
            if (GameObject* object = m_slots[i].object.get())
                f(*object);
    }
    void ResolveCollisions();

    InitArgs m_initArgs = {};
    std::vector<Slot> m_slots;
    std::vector<uint32_t> m_freeSlots;
    std::vector<std::pair<uint32_t, std::unique_ptr<GameObject>>> m_pendingSpawn;   // 槽位在 Spawn 时就预留
    std::vector<ObjectHandle> m_pendingDestroy;

    CollisionSystem m_collisions;
    EventBus m_events;
};
