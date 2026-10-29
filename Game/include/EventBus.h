#pragma once
#include <functional>
#include <typeindex>
#include <unordered_map>
#include <vector>

// 事件总线：发布方不需要知道谁在监听。
// Publish 只把事件放进队列，由 Scene 在每帧固定的时间点调用 Dispatch 统一分发，
// 避免在某个对象 Update 执行到一半时被别的对象改掉状态。
class EventBus
{
public:
    // owner 用于退订；对象销毁时 Scene 会自动退订它的所有监听
    template<class E>
    void Subscribe(const void* owner, std::function<void(const E&)> handler)
    {
        m_subscribers[typeid(E)].push_back({ owner, [handler = std::move(handler)](const void* e) {
            handler(*static_cast<const E*>(e));
        } });
    }

    template<class E>
    void Publish(const E& event)
    {
        m_queue.push_back([this, event] { Deliver(typeid(E), &event); });
    }

    void Unsubscribe(const void* owner);

    // 分发队列中的事件；分发过程中新发布的事件在同一帧内继续分发（有轮数上限，防止事件互相触发死循环）
    void Dispatch();
    void Clear();

private:
    struct Subscriber
    {
        const void* owner;
        std::function<void(const void*)> handler;
    };

    void Deliver(std::type_index type, const void* event);

    std::unordered_map<std::type_index, std::vector<Subscriber>> m_subscribers;
    std::vector<std::function<void()>> m_queue;
};
