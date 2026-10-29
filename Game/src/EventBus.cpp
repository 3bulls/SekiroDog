#include "EventBus.h"

#include "Log.h"

void EventBus::Unsubscribe(const void* owner)
{
    for (auto& [type, subscribers] : m_subscribers)
        std::erase_if(subscribers, [owner](const Subscriber& s) { return s.owner == owner; });
}

void EventBus::Dispatch()
{
    constexpr int MaxRounds = 8;
    for (int round = 0; round < MaxRounds && !m_queue.empty(); ++round)
    {
        std::vector<std::function<void()>> pending;
        pending.swap(m_queue);
        for (const auto& deliver : pending)
            deliver();
    }
    if (!m_queue.empty())
    {
        Log("EventBus: %zu events still pending after %d rounds, deferred to next frame", m_queue.size(), MaxRounds);
    }
}

void EventBus::Clear()
{
    m_queue.clear();
    m_subscribers.clear();
}

void EventBus::Deliver(std::type_index type, const void* event)
{
    auto it = m_subscribers.find(type);
    if (it == m_subscribers.end())
        return;
    // 拷贝一份，允许处理函数里订阅 / 退订
    const std::vector<Subscriber> subscribers = it->second;
    for (const Subscriber& s : subscribers)
        s.handler(event);
}
