#pragma once
#include <cassert>
#include <memory>
#include <vector>
#include "GameObject.h"

// 通用有限状态机。Owner 是状态所属的对象，Id 是状态枚举（需从 0 连续编号）。
// 状态切换用 ChangeState 请求，在当前状态 Update 结束后才生效：
// 旧状态 Exit → 新状态 Enter，避免在 Update 执行到一半时切换。
template<class Owner, class Id>
class StateMachine
{
public:
    class State
    {
    public:
        virtual ~State() = default;
        virtual const char* Name() const = 0;
        virtual void Enter(Owner&) {}
        virtual void Update(Owner& owner, const UpdateArgs& args) = 0;
        virtual void Exit(Owner&) {}
    };

    void Add(Id id, std::unique_ptr<State> state)
    {
        const size_t i = static_cast<size_t>(id);
        if (m_states.size() <= i)
            m_states.resize(i + 1);
        m_states[i] = std::move(state);
    }

    void Start(Owner& owner, Id id)
    {
        m_current = id;
        m_hasPending = false;
        m_started = true;
        Get(id).Enter(owner);
    }

    // 返回 true 表示本帧发生了切换
    bool Update(Owner& owner, const UpdateArgs& args)
    {
        Get(m_current).Update(owner, args);
        if (!m_hasPending)
            return false;
        m_hasPending = false;
        if (m_pending == m_current)
            return false;
        Get(m_current).Exit(owner);
        m_previous = m_current;
        m_current = m_pending;
        Get(m_current).Enter(owner);
        return true;
    }

    void ChangeState(Id id)
    {
        m_pending = id;
        m_hasPending = true;
    }

    void Clear()
    {
        m_states.clear();
        m_started = false;
        m_hasPending = false;
    }

    Id Current() const { return m_current; }
    Id Previous() const { return m_previous; }
    const char* CurrentName() const { return m_started ? Get(m_current).Name() : ""; }
    const char* PreviousName() const { return m_started ? Get(m_previous).Name() : ""; }

private:
    State& Get(Id id) const
    {
        const size_t i = static_cast<size_t>(id);
        assert(i < m_states.size() && m_states[i]);
        return *m_states[i];
    }

    std::vector<std::unique_ptr<State>> m_states;
    Id m_current{};
    Id m_previous{};
    Id m_pending{};
    bool m_hasPending = false;
    bool m_started = false;
};
