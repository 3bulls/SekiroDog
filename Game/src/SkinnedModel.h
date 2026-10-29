#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <d3d11.h>
#include <wrl/client.h>
#include <BufferHelpers.h>
#include <SimpleMath.h>
#include "Graphics.h"

struct aiNode;
struct SkinnedVertex;

struct SkinnedModelDesc
{
    std::string fbxPath;
    std::wstring baseColorPath;
    std::wstring normalPath;
    std::wstring metallicSmoothnessPath;
    std::wstring shaderPath;
};

// 从 FBX 读取的蒙皮模型：GPU 蒙皮 + CPU 采样骨骼动画
class SkinnedModel
{
public:
    static constexpr int MaxBones = 128;

    bool Load(ID3D11Device* device, ID3D11DeviceContext* context, const SkinnedModelDesc& desc);

    // 返回名字包含 nameContains 的动画片段索引，找不到返回 -1
    int FindClip(const std::string& nameContains) const;

    // 采样 clipA 和 clipB，按 blend（0 = A，1 = B）混合后更新骨骼矩阵
    void UpdatePose(int clipA, float timeA, int clipB, float timeB, float blend);

    void Draw(ID3D11DeviceContext* context, const DirectX::SimpleMath::Matrix& world,
              DirectX::ConstantBuffer<ObjectConstants>& objectCB);

private:
    using Vector3 = DirectX::SimpleMath::Vector3;
    using Quaternion = DirectX::SimpleMath::Quaternion;
    using Matrix = DirectX::SimpleMath::Matrix;

    struct TRS { Vector3 t; Quaternion r; Vector3 s; };
    struct Node { std::string name; int parent; };
    struct VecKey { double time; Vector3 value; };
    struct QuatKey { double time; Quaternion value; };
    struct Channel
    {
        int node;
        std::vector<VecKey> position;
        std::vector<QuatKey> rotation;
        std::vector<VecKey> scale;
    };
    struct Clip
    {
        std::string name;
        double start = 0.0;
        double end = 0.0;
        double ticksPerSecond = 30.0;
        std::vector<Channel> channels;
    };
    struct BonesConstants { DirectX::XMFLOAT4X4 bones[MaxBones]; };

    void AddNode(const aiNode* node, int parent);
    int FindNode(const std::string& name) const;
    void SampleClip(const Clip& clip, float seconds, std::vector<TRS>& pose) const;
    void ComputeGlobals(const std::vector<TRS>& pose);
    void ComputeModelFix(const std::vector<SkinnedVertex>& vertices);
    bool CreateGpuResources(ID3D11Device* device, ID3D11DeviceContext* context, const SkinnedModelDesc& desc,
                            const std::vector<SkinnedVertex>& vertices, const std::vector<uint32_t>& indices);

    // 骨架与动画
    std::vector<Node> m_nodes;                 // 父节点总在子节点之前
    std::vector<TRS> m_bindPose;
    std::unordered_map<std::string, int> m_nodeIndex;
    std::vector<int> m_boneNode;
    std::vector<Matrix> m_boneOffset;
    std::vector<Clip> m_clips;

    // 每帧临时数据
    std::vector<TRS> m_poseA, m_poseB;
    std::vector<Matrix> m_globals;
    std::vector<Matrix> m_boneMatrices;
    Matrix m_modelFix;                         // 朝向 +Z、脚底在 y = 0

    // GPU 资源
    ComPtr<ID3D11Buffer> m_vertexBuffer;
    ComPtr<ID3D11Buffer> m_indexBuffer;
    UINT m_indexCount = 0;
    ComPtr<ID3D11VertexShader> m_vs;
    ComPtr<ID3D11PixelShader> m_ps;
    ComPtr<ID3D11InputLayout> m_layout;
    ComPtr<ID3D11ShaderResourceView> m_textures[3];
    ComPtr<ID3D11SamplerState> m_sampler;
    ComPtr<ID3D11RasterizerState> m_rasterizer;
    DirectX::ConstantBuffer<BonesConstants> m_bonesCB;
    BonesConstants m_bonesData = {};
};
