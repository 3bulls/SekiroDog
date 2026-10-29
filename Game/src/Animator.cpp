#include "Animator.h"

#include <algorithm>
#include "SkinnedModel.h"

void Animator::Play(int clip, float fadeTime)
{
    if (clip == m_current)
        return;
    m_previous = m_current;
    m_previousTime = m_currentTime;
    m_current = clip;
    m_currentTime = 0.0f;
    m_fadeTime = fadeTime;
    m_fade = (m_previous < 0 || fadeTime <= 0.0f) ? 1.0f : 0.0f;
}

void Animator::Update(float dt)
{
    m_currentTime += dt;
    m_previousTime += dt;
    if (m_fade < 1.0f)
        m_fade = std::min(1.0f, m_fade + dt / m_fadeTime);

    if (!m_model)
        return;
    if (m_fade >= 1.0f)
        m_model->UpdatePose(m_current, m_currentTime, -1, 0.0f, 0.0f);
    else
        m_model->UpdatePose(m_previous, m_previousTime, m_current, m_currentTime, m_fade);
}
