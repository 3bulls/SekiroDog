#pragma once

class SkinnedModel;

// 动画播放器：播放一个片段，切换时从当前片段淡入到新片段
class Animator
{
public:
    void Initialize(SkinnedModel* model) { m_model = model; }
    void Finalize() { m_model = nullptr; m_current = m_previous = -1; }

    // 从头播放 clip，fadeTime 秒内从当前片段过渡过去；已在播放同一片段时不做任何事
    void Play(int clip, float fadeTime);
    void Update(float dt);                  // 推进时间并更新模型骨骼

    int CurrentClip() const { return m_current; }
    float CurrentTime() const { return m_currentTime; }

private:
    SkinnedModel* m_model = nullptr;
    int m_current = -1;
    int m_previous = -1;
    float m_currentTime = 0.0f;
    float m_previousTime = 0.0f;
    float m_fade = 1.0f;                    // 0 = 完全是上一个片段，1 = 完全是当前片段
    float m_fadeTime = 0.0f;
};
