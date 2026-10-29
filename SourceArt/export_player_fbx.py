# 将 Dog_Warrior 源文件导出为 Unity 可用的 FBX + 贴图。
# 用法（在工程根目录执行）:
#   blender -b SourceArt/Dog_Warrior_Fast_Run_HandsFixed.blend --python SourceArt/export_player_fbx.py
# 不会修改 .blend 源文件（不保存）。
import os
import bpy
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MODEL_DIR = os.path.join(ROOT, "Assets", "Player", "Models")
TEX_DIR = os.path.join(ROOT, "Assets", "Player", "Textures")
FBX_PATH = os.path.join(MODEL_DIR, "DogWarrior.fbx")

RIG_NAME = "Dog_Warrior_Rig"
MESH_NAME = "Dog_Warrior_Mesh"
TARGET_VERTS = 50000                       # 原始网格约 96 万顶点，减面到游戏可用规模
# "Idle_Generated" 由 make_idle_action() 生成（源文件里没有完整的待机动画）
CLIPS = {"Run": "Fast_Run_InPlace", "Idle": "Idle_Generated"}

os.makedirs(MODEL_DIR, exist_ok=True)
os.makedirs(TEX_DIR, exist_ok=True)

rig = bpy.data.objects[RIG_NAME]
mesh = bpy.data.objects[MESH_NAME]


def make_idle_action(rig, run_action_name, frames=60):
    """站立待机：手臂自然下垂、握拳，脊柱轻微呼吸起伏，循环 frames 帧。"""
    from math import radians, sin, pi
    from mathutils import Quaternion, Vector

    scene = bpy.context.scene
    ad = rig.animation_data
    run = bpy.data.actions[run_action_name]

    # 从跑步第 1 帧取 IK/FK 开关和手指（握拳）姿势
    ad.action = run
    if hasattr(ad, "action_slot") and run.slots:
        ad.action_slot = run.slots[0]
    scene.frame_set(1)
    ik_fk = {pb.name: pb["ik_fk"] for pb in rig.pose.bones if "ik_fk" in pb.keys()}
    fingers = ("thumb", "index", "middle", "ring", "pinky")
    finger_rot = {pb.name: pb.rotation_quaternion.copy() for pb in rig.pose.bones if pb.name.startswith(fingers)}

    idle = bpy.data.actions.new("Idle_Generated")
    ad.action = idle
    for pb in rig.pose.bones:
        pb.location = (0, 0, 0)
        pb.rotation_mode = "QUATERNION"
        pb.rotation_quaternion = (1, 0, 0, 0)
        pb.scale = (1, 1, 1)
    for name, value in ik_fk.items():
        rig.pose.bones[name]["ik_fk"] = value
    for name, q in finger_rot.items():
        rig.pose.bones[name].rotation_quaternion = q

    def aim(bone_name, direction):
        """旋转 FK 骨骼，使其 Y 轴（骨骼方向）在骨架空间指向 direction。"""
        bpy.context.view_layer.update()
        pb = rig.pose.bones[bone_name]
        without_basis = pb.matrix @ pb.matrix_basis.inverted()
        local = (without_basis.to_3x3().inverted() @ Vector(direction)).normalized()
        pb.rotation_quaternion = Vector((0, 1, 0)).rotation_difference(local)

    # Blender 中角色面朝 -Y，左侧为 +X
    for side, sx in (("L", 1.0), ("R", -1.0)):
        aim(f"upper_arm_fk.{side}", (0.28 * sx, 0.02, -1.0))
        aim(f"forearm_fk.{side}", (0.12 * sx, -0.45, -1.0))
    bpy.context.view_layer.update()
    arm_pose = {pb.name: pb.rotation_quaternion.copy()
                for pb in rig.pose.bones if pb.name.startswith(("upper_arm_fk", "forearm_fk"))}

    for f in range(1, frames + 2, 10):
        phase = sin(2 * pi * (f - 1) / frames)
        breath = Quaternion((1, 0, 0), radians(1.5) * phase)
        for name in ("spine.02", "spine.03"):
            pb = rig.pose.bones[name]
            pb.rotation_quaternion = breath
            pb.keyframe_insert("rotation_quaternion", frame=f)
        sway = Quaternion((1, 0, 0), radians(2.0) * phase)
        for name, q in arm_pose.items():
            pb = rig.pose.bones[name]
            pb.rotation_quaternion = (sway @ q) if name.startswith("upper_arm") else q
            pb.keyframe_insert("rotation_quaternion", frame=f)
        for name in list(finger_rot) + ["root", "torso"]:
            pb = rig.pose.bones[name]
            pb.keyframe_insert("rotation_quaternion", frame=f)
            pb.keyframe_insert("location", frame=f)
        for name in ik_fk:
            rig.pose.bones[name].keyframe_insert('["ik_fk"]', frame=f)
    return idle


make_idle_action(rig, CLIPS["Run"])

# 1. 只保留角色骨架和网格（删除 Mixamo 源骨架、灯光、相机、控件形状等）
for obj in list(bpy.data.objects):
    if obj not in (rig, mesh):
        bpy.data.objects.remove(obj, do_unlink=True)

# 2. 减面：Decimate 放在修改器栈最前面并应用，顶点权重会被插值保留
bpy.context.view_layer.objects.active = mesh
dec = mesh.modifiers.new("Decimate", "DECIMATE")
dec.ratio = min(1.0, TARGET_VERTS / len(mesh.data.vertices))
dec.use_collapse_triangulate = True
with bpy.context.temp_override(object=mesh, active_object=mesh):
    bpy.ops.object.modifier_move_to_index(modifier=dec.name, index=0)
    bpy.ops.object.modifier_apply(modifier=dec.name)
print("decimated verts:", len(mesh.data.vertices))

# 3. 导出贴图（文件里是打包贴图）
def image_of(socket_name):
    mat = mesh.data.materials[0]
    bsdf = next(n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    link = bsdf.inputs[socket_name].links[0]
    node = link.from_node
    if node.type == "NORMAL_MAP":
        node = node.inputs["Color"].links[0].from_node
    return node.image

def save_image(img, name, fmt):
    img.filepath_raw = os.path.join(TEX_DIR, name)
    img.file_format = fmt
    img.save()

save_image(image_of("Base Color"), "DogWarrior_BaseColor.png", "PNG")
save_image(image_of("Normal"), "DogWarrior_Normal.png", "PNG")

# URP Lit 用 R=Metallic, A=Smoothness(1-Roughness)
metal, rough = image_of("Metallic"), image_of("Roughness")
w, h = metal.size
mp = np.empty(w * h * 4, dtype=np.float32); metal.pixels.foreach_get(mp)
if tuple(rough.size) != (w, h):
    rough.scale(w, h)
rp = np.empty(w * h * 4, dtype=np.float32); rough.pixels.foreach_get(rp)
out = np.zeros((w * h, 4), dtype=np.float32)
out[:, 0] = mp.reshape(-1, 4)[:, 0]
out[:, 3] = 1.0 - rp.reshape(-1, 4)[:, 0]
ms = bpy.data.images.new("DogWarrior_MetallicSmoothness", w, h, alpha=True)
ms.colorspace_settings.name = "Non-Color"
ms.pixels.foreach_set(out.ravel())
save_image(ms, "DogWarrior_MetallicSmoothness.png", "PNG")

# 4. 动画：每个片段一条 NLA 轨道，FBX 中的 take 名即片段名
rig.animation_data.action = None
for track in list(rig.animation_data.nla_tracks):
    rig.animation_data.nla_tracks.remove(track)
for clip_name, action_name in CLIPS.items():
    action = bpy.data.actions[action_name]
    track = rig.animation_data.nla_tracks.new()
    track.name = clip_name
    strip = track.strips.new(clip_name, int(action.frame_range[0]), action)
    if hasattr(strip, "action_slot") and action.slots:
        strip.action_slot = action.slots[0]

# 5. 导出 FBX（Unity 坐标：Y 朝上；只导出变形骨骼）
bpy.ops.object.select_all(action="DESELECT")
rig.select_set(True)
mesh.select_set(True)
bpy.context.view_layer.objects.active = rig
bpy.ops.export_scene.fbx(
    filepath=FBX_PATH,
    use_selection=True,
    object_types={"ARMATURE", "MESH"},
    apply_scale_options="FBX_SCALE_ALL",
    axis_forward="-Z",
    axis_up="Y",
    use_mesh_modifiers=False,
    mesh_smooth_type="FACE",
    use_tspace=True,
    add_leaf_bones=False,
    use_armature_deform_only=True,
    armature_nodetype="NULL",
    bake_anim=True,
    bake_anim_use_all_bones=True,
    bake_anim_use_nla_strips=True,
    bake_anim_use_all_actions=False,
    bake_anim_force_startend_keying=True,
    bake_anim_simplify_factor=0.0,
    path_mode="STRIP",
    embed_textures=False,
)
print("exported:", FBX_PATH)
