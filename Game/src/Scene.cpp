#include "Scene.h"

#include <ranges>
#include "Log.h"

bool Scene::Initialize(const InitArgs& args)
{
    m_initArgs = args;
    return Commit();
}

void Scene::Update(const UpdateArgs& args)
{
    ForEach([&](GameObject& object) { object.Update(args); });
    ResolveCollisions();
    m_events.Dispatch();
    ForEach([&](GameObject& object) { object.LateUpdate(args); });
    Commit();
}

void Scene::Draw(const DrawArgs& args)
{
    ForEach([&](GameObject& object) { object.Draw(args); });
}

void Scene::Finalize()
{
    m_pendingSpawn.clear();
    m_pendingDestroy.clear();
    for (Slot& slot : m_slots | std::views::reverse)
        if (slot.object)
        {
            slot.object->Finalize();
            slot.object.reset();
        }
    m_slots.clear();
    m_freeSlots.clear();
    m_events.Clear();
    m_collisions.Clear();
}

ObjectHandle Scene::Spawn(std::unique_ptr<GameObject> object)
{
    uint32_t index;
    if (!m_freeSlots.empty())
    {
        index = m_freeSlots.back();
        m_freeSlots.pop_back();
    }
    else
    {
        index = static_cast<uint32_t>(m_slots.size());
        m_slots.emplace_back();
    }
    const ObjectHandle handle{ index, m_slots[index].generation };
    object->m_scene = this;
    object->m_handle = handle;
    m_pendingSpawn.emplace_back(index, std::move(object));
    return handle;
}

void Scene::Destroy(ObjectHandle handle)
{
    if (Get(handle))
        m_pendingDestroy.push_back(handle);
}

bool Scene::Commit()
{
    std::vector<ObjectHandle> destroy;
    destroy.swap(m_pendingDestroy);
    for (ObjectHandle handle : destroy)
        if (GameObject* object = Get(handle))
        {
            object->Finalize();
            m_events.Unsubscribe(object);
            Slot& slot = m_slots[handle.index];
            slot.object.reset();
            ++slot.generation;
            m_freeSlots.push_back(handle.index);
        }

    // 先全部放入槽位再逐个 Initialize，同一批的对象可以互相 Find
    decltype(m_pendingSpawn) spawn;
    spawn.swap(m_pendingSpawn);
    for (auto& [index, object] : spawn)
        m_slots[index].object = std::move(object);

    bool ok = true;
    for (const auto& entry : spawn)
    {
        GameObject& object = *m_slots[entry.first].object;
        if (!object.Initialize(m_initArgs))
        {
            Log("Scene: object in slot %u failed to initialize", entry.first);
            ok = false;
        }
    }
    return ok;
}

GameObject* Scene::Get(ObjectHandle handle) const
{
    if (handle.index >= m_slots.size())
        return nullptr;
    const Slot& slot = m_slots[handle.index];
    return slot.generation == handle.generation ? slot.object.get() : nullptr;
}

void Scene::ResolveCollisions()
{
    // 先收集全部结果再回调，回调里移动物体不会影响本轮检测
    struct Hit { ObjectHandle a, b; Contact contact; };
    std::vector<Hit> hits;
    m_collisions.Detect([&](const Collider& a, const Collider& b, const Contact& contact) {
        hits.push_back({ a.owner, b.owner, contact });
    });

    for (const Hit& hit : hits)
    {
        GameObject* a = Get(hit.a);
        GameObject* b = Get(hit.b);
        if (!a || !b)
            continue;
        a->OnCollision(*b, hit.contact);

        Contact reversed = hit.contact;
        reversed.normal = -reversed.normal;
        std::swap(reversed.self, reversed.other);
        b->OnCollision(*a, reversed);
    }
}
