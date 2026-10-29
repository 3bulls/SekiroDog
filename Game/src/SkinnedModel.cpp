#include "SkinnedModel.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <WICTextureLoader.h>
#include "Log.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

struct SkinnedVertex
{
    XMFLOAT3 position;
    XMFLOAT3 normal;
    XMFLOAT4 tangent;     // w = 副切线方向
    XMFLOAT2 uv;
    uint8_t bones[4];
    float weights[4];
};

namespace
{
    // Assimp 是列向量矩阵，DirectXMath 是行向量矩阵：转置
    Matrix ToMatrix(const aiMatrix4x4& m)
    {
        return Matrix(m.a1, m.b1, m.c1, m.d1,
                      m.a2, m.b2, m.c2, m.d2,
                      m.a3, m.b3, m.c3, m.d3,
                      m.a4, m.b4, m.c4, m.d4);
    }

    Vector3 ToVector(const aiVector3D& v) { return Vector3(v.x, v.y, v.z); }
    Quaternion ToQuaternion(const aiQuaternion& q) { return Quaternion(q.x, q.y, q.z, q.w); }

    // 最后一个 time <= t 的关键帧
    template <typename Key>
    size_t KeyIndex(const std::vector<Key>& keys, double t)
    {
        auto it = std::upper_bound(keys.begin(), keys.end(), t,
                                   [](double v, const Key& k) { return v < k.time; });
        return it == keys.begin() ? 0 : static_cast<size_t>(it - keys.begin() - 1);
    }

    template <typename Key>
    float KeyFraction(const std::vector<Key>& keys, size_t i, double t)
    {
        double span = keys[i + 1].time - keys[i].time;
        return span > 0.0 ? std::clamp(static_cast<float>((t - keys[i].time) / span), 0.0f, 1.0f) : 0.0f;
    }

    template <typename Key, typename T, typename Interp>
    T SampleKeys(const std::vector<Key>& keys, double t, const T& fallback, Interp interp)
    {
        if (keys.empty())
            return fallback;
        size_t i = KeyIndex(keys, t);
        if (i + 1 >= keys.size())
            return keys[i].value;
        return interp(keys[i].value, keys[i + 1].value, KeyFraction(keys, i, t));
    }

    bool LoadTexture(ID3D11Device* device, ID3D11DeviceContext* context, const std::wstring& path, bool srgb,
                     ComPtr<ID3D11ShaderResourceView>& srv)
    {
        HRESULT hr = CreateWICTextureFromFileEx(device, context, path.c_str(), 0, D3D11_USAGE_DEFAULT,
                                                D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET, 0,
                                                D3D11_RESOURCE_MISC_GENERATE_MIPS,
                                                srgb ? WIC_LOADER_FORCE_SRGB : WIC_LOADER_IGNORE_SRGB,
                                                nullptr, srv.ReleaseAndGetAddressOf());
        if (FAILED(hr))
            Log("Failed to load texture %ls (0x%08X)", path.c_str(), static_cast<unsigned>(hr));
        return SUCCEEDED(hr);
    }
}

bool SkinnedModel::Load(ID3D11Device* device, ID3D11DeviceContext* context, const SkinnedModelDesc& desc)
{
    Assimp::Importer importer;
    importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);
    const aiScene* scene = importer.ReadFile(desc.fbxPath,
        aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace |
        aiProcess_JoinIdenticalVertices | aiProcess_LimitBoneWeights | aiProcess_FlipUVs);
    if (!scene || !scene->mRootNode)
    {
        Log("Assimp failed to load %s: %s", desc.fbxPath.c_str(), importer.GetErrorString());
        return false;
    }

    AddNode(scene->mRootNode, -1);

    // 合并所有网格到一个顶点缓冲，骨骼按名字全局编号
    std::vector<SkinnedVertex> vertices;
    std::vector<uint32_t> indices;
    std::unordered_map<std::string, int> boneIndex;
    for (unsigned m = 0; m < scene->mNumMeshes; ++m)
    {
        const aiMesh* mesh = scene->mMeshes[m];
        const uint32_t base = static_cast<uint32_t>(vertices.size());
        vertices.resize(base + mesh->mNumVertices);

        for (unsigned i = 0; i < mesh->mNumVertices; ++i)
        {
            SkinnedVertex& v = vertices[base + i];
            v = {};
            v.position = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
            v.normal = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };
            if (mesh->HasTangentsAndBitangents())
            {
                Vector3 n = ToVector(mesh->mNormals[i]);
                Vector3 t = ToVector(mesh->mTangents[i]);
                Vector3 b = ToVector(mesh->mBitangents[i]);
                float sign = n.Cross(t).Dot(b) < 0.0f ? -1.0f : 1.0f;
                v.tangent = { t.x, t.y, t.z, sign };
            }
            if (mesh->HasTextureCoords(0))
                v.uv = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };
        }

        for (unsigned b = 0; b < mesh->mNumBones; ++b)
        {
            const aiBone* bone = mesh->mBones[b];
            auto [it, inserted] = boneIndex.try_emplace(bone->mName.C_Str(), static_cast<int>(m_boneNode.size()));
            if (inserted)
            {
                m_boneNode.push_back(FindNode(bone->mName.C_Str()));
                m_boneOffset.push_back(ToMatrix(bone->mOffsetMatrix));
                if (m_boneNode.back() < 0)
                    Log("Bone without node: %s", bone->mName.C_Str());
            }
            const int bi = it->second;

            for (unsigned w = 0; w < bone->mNumWeights; ++w)
            {
                SkinnedVertex& v = vertices[base + bone->mWeights[w].mVertexId];
                int slot = static_cast<int>(std::min_element(v.weights, v.weights + 4) - v.weights);
                if (bone->mWeights[w].mWeight > v.weights[slot])
                {
                    v.bones[slot] = static_cast<uint8_t>(bi);
                    v.weights[slot] = bone->mWeights[w].mWeight;
                }
            }
        }

        for (unsigned f = 0; f < mesh->mNumFaces; ++f)
            for (unsigned k = 0; k < mesh->mFaces[f].mNumIndices; ++k)
                indices.push_back(base + mesh->mFaces[f].mIndices[k]);
    }

    if (m_boneNode.size() > MaxBones)
    {
        Log("Too many bones: %zu (max %d)", m_boneNode.size(), MaxBones);
        return false;
    }
    for (SkinnedVertex& v : vertices)
    {
        float sum = v.weights[0] + v.weights[1] + v.weights[2] + v.weights[3];
        if (sum > 0.0f)
            for (float& w : v.weights) w /= sum;
        else
            v.weights[0] = 1.0f;
    }

    for (unsigned a = 0; a < scene->mNumAnimations; ++a)
    {
        const aiAnimation* anim = scene->mAnimations[a];
        Clip clip;
        clip.name = anim->mName.C_Str();
        clip.ticksPerSecond = anim->mTicksPerSecond > 0.0 ? anim->mTicksPerSecond : 30.0;
        clip.start = DBL_MAX;
        clip.end = -DBL_MAX;
        for (unsigned c = 0; c < anim->mNumChannels; ++c)
        {
            const aiNodeAnim* src = anim->mChannels[c];
            Channel ch;
            ch.node = FindNode(src->mNodeName.C_Str());
            if (ch.node < 0)
                continue;
            for (unsigned k = 0; k < src->mNumPositionKeys; ++k)
                ch.position.push_back({ src->mPositionKeys[k].mTime, ToVector(src->mPositionKeys[k].mValue) });
            for (unsigned k = 0; k < src->mNumRotationKeys; ++k)
                ch.rotation.push_back({ src->mRotationKeys[k].mTime, ToQuaternion(src->mRotationKeys[k].mValue) });
            for (unsigned k = 0; k < src->mNumScalingKeys; ++k)
                ch.scale.push_back({ src->mScalingKeys[k].mTime, ToVector(src->mScalingKeys[k].mValue) });
            for (const auto* keys : { &ch.position, &ch.scale })
                if (!keys->empty())
                {
                    clip.start = std::min(clip.start, keys->front().time);
                    clip.end = std::max(clip.end, keys->back().time);
                }
            if (!ch.rotation.empty())
            {
                clip.start = std::min(clip.start, ch.rotation.front().time);
                clip.end = std::max(clip.end, ch.rotation.back().time);
            }
            clip.channels.push_back(std::move(ch));
        }
        if (clip.start > clip.end)
        {
            clip.start = 0.0;
            clip.end = anim->mDuration;
        }
        Log("Clip '%s': %.3f s, %zu channels", clip.name.c_str(),
            (clip.end - clip.start) / clip.ticksPerSecond, clip.channels.size());
        m_clips.push_back(std::move(clip));
    }

    Log("Loaded %s: %zu vertices, %zu triangles, %zu bones, %zu nodes", desc.fbxPath.c_str(),
        vertices.size(), indices.size() / 3, m_boneNode.size(), m_nodes.size());

    m_poseA = m_poseB = m_bindPose;
    m_globals.resize(m_nodes.size());
    m_boneMatrices.resize(m_boneNode.size());
    ComputeModelFix(vertices);
    return CreateGpuResources(device, context, desc, vertices, indices);
}

void SkinnedModel::AddNode(const aiNode* node, int parent)
{
    const int index = static_cast<int>(m_nodes.size());
    m_nodes.push_back({ node->mName.C_Str(), parent });
    m_nodeIndex[node->mName.C_Str()] = index;

    TRS bind;
    ToMatrix(node->mTransformation).Decompose(bind.s, bind.r, bind.t);
    m_bindPose.push_back(bind);

    for (unsigned i = 0; i < node->mNumChildren; ++i)
        AddNode(node->mChildren[i], index);
}

int SkinnedModel::FindNode(const std::string& name) const
{
    auto it = m_nodeIndex.find(name);
    return it == m_nodeIndex.end() ? -1 : it->second;
}

int SkinnedModel::FindClip(const std::string& nameContains) const
{
    for (size_t i = 0; i < m_clips.size(); ++i)
        if (m_clips[i].name.find(nameContains) != std::string::npos)
            return static_cast<int>(i);
    return -1;
}

void SkinnedModel::SampleClip(const Clip& clip, float seconds, std::vector<TRS>& pose) const
{
    const double length = clip.end - clip.start;
    double ticks = clip.start;
    if (length > 0.0)
        ticks += std::fmod(static_cast<double>(seconds) * clip.ticksPerSecond, length);

    for (const Channel& ch : clip.channels)
    {
        TRS& x = pose[ch.node];
        x.t = SampleKeys(ch.position, ticks, x.t, [](const Vector3& a, const Vector3& b, float f) { return Vector3::Lerp(a, b, f); });
        x.r = SampleKeys(ch.rotation, ticks, x.r, [](const Quaternion& a, const Quaternion& b, float f) { return Quaternion::Slerp(a, b, f); });
        x.s = SampleKeys(ch.scale, ticks, x.s, [](const Vector3& a, const Vector3& b, float f) { return Vector3::Lerp(a, b, f); });
    }
}

void SkinnedModel::ComputeGlobals(const std::vector<TRS>& pose)
{
    for (size_t i = 0; i < m_nodes.size(); ++i)
    {
        const TRS& x = pose[i];
        Matrix local = Matrix::CreateScale(x.s) * Matrix::CreateFromQuaternion(x.r) * Matrix::CreateTranslation(x.t);
        const int parent = m_nodes[i].parent;
        m_globals[i] = parent < 0 ? local : local * m_globals[parent];
    }
    for (size_t b = 0; b < m_boneNode.size(); ++b)
        m_boneMatrices[b] = m_boneNode[b] < 0 ? Matrix::Identity : m_boneOffset[b] * m_globals[m_boneNode[b]];
}

void SkinnedModel::UpdatePose(int clipA, float timeA, int clipB, float timeB, float blend)
{
    std::copy(m_bindPose.begin(), m_bindPose.end(), m_poseA.begin());
    if (clipA >= 0)
        SampleClip(m_clips[clipA], timeA, m_poseA);

    if (clipB >= 0 && blend > 0.0f)
    {
        std::copy(m_bindPose.begin(), m_bindPose.end(), m_poseB.begin());
        SampleClip(m_clips[clipB], timeB, m_poseB);
        for (size_t i = 0; i < m_poseA.size(); ++i)
        {
            m_poseA[i].t = Vector3::Lerp(m_poseA[i].t, m_poseB[i].t, blend);
            m_poseA[i].r = Quaternion::Slerp(m_poseA[i].r, m_poseB[i].r, blend);
            m_poseA[i].s = Vector3::Lerp(m_poseA[i].s, m_poseB[i].s, blend);
        }
    }
    ComputeGlobals(m_poseA);
}

void SkinnedModel::ComputeModelFix(const std::vector<SkinnedVertex>& vertices)
{
    ComputeGlobals(m_bindPose);

    // 绑定姿势下的包围盒
    Vector3 mn(FLT_MAX), mx(-FLT_MAX);
    for (const SkinnedVertex& v : vertices)
    {
        Matrix skin = m_boneMatrices[v.bones[0]] * v.weights[0] + m_boneMatrices[v.bones[1]] * v.weights[1]
                    + m_boneMatrices[v.bones[2]] * v.weights[2] + m_boneMatrices[v.bones[3]] * v.weights[3];
        Vector3 p = Vector3::Transform(Vector3(v.position.x, v.position.y, v.position.z), skin);
        mn = Vector3::Min(mn, p);
        mx = Vector3::Max(mx, p);
    }
    Log("Bind pose bounds min (%.3f, %.3f, %.3f) max (%.3f, %.3f, %.3f)", mn.x, mn.y, mn.z, mx.x, mx.y, mx.z);

    // 单位修正：模型约 0.8 米高，若导入成厘米则缩小。
    // 朝向无需修正：export_player_fbx.py 导出的模型（Blender 中面朝 -Y）在这里面朝 +Z。
    float height = mx.y - mn.y;
    float scale = height > 20.0f ? 0.01f : 1.0f;
    Log("Model fix: scale %.3f, height %.3f m", scale, height * scale);

    m_modelFix = Matrix::CreateScale(scale) * Matrix::CreateTranslation(0.0f, -mn.y * scale, 0.0f);
}

bool SkinnedModel::CreateGpuResources(ID3D11Device* device, ID3D11DeviceContext* context, const SkinnedModelDesc& desc,
                                      const std::vector<SkinnedVertex>& vertices, const std::vector<uint32_t>& indices)
{
    if (FAILED(CreateStaticBuffer(device, vertices, D3D11_BIND_VERTEX_BUFFER, &m_vertexBuffer)) ||
        FAILED(CreateStaticBuffer(device, indices, D3D11_BIND_INDEX_BUFFER, &m_indexBuffer)))
    {
        Log("Failed to create model buffers");
        return false;
    }
    m_indexCount = static_cast<UINT>(indices.size());

    auto vsCode = CompileShader(desc.shaderPath, "VSMain", "vs_5_0");
    auto psCode = CompileShader(desc.shaderPath, "PSMain", "ps_5_0");
    if (!vsCode || !psCode)
        return false;
    device->CreateVertexShader(vsCode->GetBufferPointer(), vsCode->GetBufferSize(), nullptr, &m_vs);
    device->CreatePixelShader(psCode->GetBufferPointer(), psCode->GetBufferSize(), nullptr, &m_ps);

    const D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION",     0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",       0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT",      0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",     0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "BLENDINDICES", 0, DXGI_FORMAT_R8G8B8A8_UINT,      0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "BLENDWEIGHT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    device->CreateInputLayout(layout, _countof(layout), vsCode->GetBufferPointer(), vsCode->GetBufferSize(), &m_layout);

    if (!LoadTexture(device, context, desc.baseColorPath, true, m_textures[0]) ||
        !LoadTexture(device, context, desc.normalPath, false, m_textures[1]) ||
        !LoadTexture(device, context, desc.metallicSmoothnessPath, false, m_textures[2]))
        return false;

    D3D11_SAMPLER_DESC sd = {};
    sd.Filter = D3D11_FILTER_ANISOTROPIC;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.MaxAnisotropy = 8;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    device->CreateSamplerState(&sd, &m_sampler);

    // 双面渲染：衣摆、围巾等是单层面片
    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    rd.DepthClipEnable = TRUE;
    device->CreateRasterizerState(&rd, &m_rasterizer);

    m_bonesCB.Create(device);
    return true;
}

void SkinnedModel::Draw(ID3D11DeviceContext* context, const Matrix& world, ConstantBuffer<ObjectConstants>& objectCB)
{
    for (size_t b = 0; b < m_boneMatrices.size(); ++b)
        XMStoreFloat4x4(&m_bonesData.bones[b], XMMatrixTranspose(m_boneMatrices[b]));
    m_bonesCB.SetData(context, m_bonesData);

    ObjectConstants object = {};
    XMStoreFloat4x4(&object.world, XMMatrixTranspose(m_modelFix * world));
    object.color = { 1.0f, 1.0f, 1.0f, 1.0f };
    objectCB.SetData(context, object);

    const UINT stride = sizeof(SkinnedVertex), offset = 0;
    context->IASetInputLayout(m_layout.Get());
    context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
    context->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->RSSetState(m_rasterizer.Get());

    ID3D11Buffer* vsBuffers[] = { objectCB.GetBuffer(), m_bonesCB.GetBuffer() };
    context->VSSetShader(m_vs.Get(), nullptr, 0);
    context->VSSetConstantBuffers(1, 2, vsBuffers);
    context->PSSetShader(m_ps.Get(), nullptr, 0);
    context->PSSetConstantBuffers(1, 1, vsBuffers);

    ID3D11ShaderResourceView* srvs[] = { m_textures[0].Get(), m_textures[1].Get(), m_textures[2].Get() };
    context->PSSetShaderResources(0, 3, srvs);
    context->PSSetSamplers(0, 1, m_sampler.GetAddressOf());

    context->DrawIndexed(m_indexCount, 0, 0);
}
