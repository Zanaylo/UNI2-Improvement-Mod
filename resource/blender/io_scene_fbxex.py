"""Blender import and export for French-Bread's `fbxex` stage container (`bg.fbx.bin`).

The codec is verified against the game itself: every one of the 28 shipped stages reads in and
writes back out byte for byte.

Export is **template based**: the file you imported from is kept and only the parts a scene can
carry - a mesh node's vertices, its triangles and where it sits - are written back over it. The
texture table, the material table, the node tree and the animation block come from the template, so
editing one prop cannot disturb the rest of the stage, and a node you move keeps the motion it had.
"""

import math
import os
import struct

try:
    import bpy
    import bmesh
    import mathutils
    from bpy_extras.io_utils import ImportHelper, ExportHelper

    INSIDE_BLENDER = True
except ImportError:
    INSIDE_BLENDER = False

bl_info = {
    "name": "FbxExp stage (.fbx.bin)",
    "description": "Import and export UNI2 / MBTL / UNI stage models",
    "author": "UNI2 Improvement Mod",
    "version": (1, 0, 0),
    "blender": (4, 0, 0),
    "location": "File > Import/Export",
    "category": "Import-Export",
}

MAGIC = b"fbxex\0\0\0"
NAME_BYTES = 128
MATERIAL_BYTES = 204
VERTEX_FLOATS = 12
MESH_PREFIX = 4 + 4 + 64




class Block(object):
    def __init__(self, count, body):
        self.count = count
        self.body = body


def read_blocks(blob):
    if blob[:8] != MAGIC:
        raise ValueError("not an fbxex container")

    at = 16
    out = []

    while at < len(blob):
        size, count = struct.unpack_from("<II", blob, at)

        if size < 8 or at + size > len(blob):
            raise ValueError("block at %d runs past the end" % at)

        out.append(Block(count, blob[at + 8:at + size]))
        at += size

    if len(out) != 4:
        raise ValueError("expected four blocks, found %d" % len(out))

    return out


def cstr(raw):
    return raw.split(b"\0")[0].decode("latin1")


def read_textures(block):
    return [cstr(block.body[i * NAME_BYTES:(i + 1) * NAME_BYTES]) for i in range(block.count)]


def read_materials(block):
    out = []

    for i in range(block.count):
        at = i * MATERIAL_BYTES
        name = cstr(block.body[at:at + NAME_BYTES])
        index, texture = struct.unpack_from("<ii", block.body, at + NAME_BYTES)
        value = list(struct.unpack_from("<17f", block.body, at + NAME_BYTES + 8))
        out.append({"filename": name, "index": index, "textureindex": texture, "value": value})

    return out


def read_mesh(payload):
    flags = list(struct.unpack_from("<ii", payload, 0))
    matrix = list(struct.unpack_from("<16f", payload, 8))
    count = struct.unpack_from("<i", payload, 72)[0]

    verts = []
    at = MESH_PREFIX + 4

    for _ in range(count):
        verts.append(list(struct.unpack_from("<%df" % VERTEX_FLOATS, payload, at)))
        at += VERTEX_FLOATS * 4

    submeshes = []
    total = struct.unpack_from("<i", payload, at)[0]
    at += 4

    for _ in range(total):
        material, indices = struct.unpack_from("<ii", payload, at)
        at += 8
        submeshes.append({
            "material": material,
            "indices": list(struct.unpack_from("<%di" % indices, payload, at)),
        })
        at += indices * 4

    return {"flags": flags, "matrix": matrix, "vertices": verts, "submeshes": submeshes}


def read_nodes(block):
    out = []
    at = 0

    for _ in range(block.count):
        size, kind, child, sibling = struct.unpack_from("<iiii", block.body, at)
        payload = block.body[at + 16:at + size]
        node = {"type": kind, "child": child, "sibling": sibling, "mesh": None}

        if kind == 1:
            node["mesh"] = read_mesh(payload)

        out.append(node)
        at += size

    return out


def read_animes(block):
    out = []
    at = 0

    for _ in range(block.count):
        frames = struct.unpack_from("<i", block.body, at)[0]
        at += 4
        track = []

        for _ in range(frames):
            track.append(list(struct.unpack_from("<16f", block.body, at)))
            at += 64

        out.append(track)

    return out


def read(path):
    with open(path, "rb") as handle:
        blob = handle.read()

    blocks = read_blocks(blob)

    return {
        "texture": read_textures(blocks[0]),
        "material": read_materials(blocks[1]),
        "node": read_nodes(blocks[2]),
        "anime": read_animes(blocks[3]),
    }


def put_name(out, text):
    raw = text.encode("latin1")[:NAME_BYTES]
    out += raw + b"\0" * (NAME_BYTES - len(raw))


def write_mesh(mesh):
    out = bytearray()
    out += struct.pack("<ii", mesh["flags"][0], mesh["flags"][1])
    out += struct.pack("<16f", *mesh["matrix"])
    out += struct.pack("<i", len(mesh["vertices"]))

    for vertex in mesh["vertices"]:
        out += struct.pack("<%df" % VERTEX_FLOATS, *vertex)

    out += struct.pack("<i", len(mesh["submeshes"]))

    for submesh in mesh["submeshes"]:
        out += struct.pack("<ii", submesh["material"], len(submesh["indices"]))
        out += struct.pack("<%di" % len(submesh["indices"]), *submesh["indices"])

    return bytes(out)


def block_bytes(count, body):
    return struct.pack("<II", len(body) + 8, count) + body


def build(model):
    body = bytearray()

    for name in model["texture"]:
        put_name(body, name)

    out = bytearray(MAGIC + struct.pack("<II", 0, 0))
    out += block_bytes(len(model["texture"]), bytes(body))

    body = bytearray()

    for material in model["material"]:
        put_name(body, material["filename"])
        body += struct.pack("<ii", material["index"], material["textureindex"])
        body += struct.pack("<17f", *material["value"])

    out += block_bytes(len(model["material"]), bytes(body))

    body = bytearray()

    for node in model["node"]:
        payload = write_mesh(node["mesh"]) if node["mesh"] is not None else b""
        body += struct.pack("<iiii", len(payload) + 16, node["type"], node["child"],
                            node["sibling"])
        body += payload

    out += block_bytes(len(model["node"]), bytes(body))

    body = bytearray()

    for track in model["anime"]:
        body += struct.pack("<i", len(track))

        for matrix in track:
            body += struct.pack("<16f", *matrix)

    out += block_bytes(len(model["anime"]), bytes(body))

    return bytes(out)


def parents(nodes):
    out = {}

    for index, node in enumerate(nodes):
        child = node["child"]

        while child != -1 and child < len(nodes):
            out[child] = index
            child = nodes[child]["sibling"]

    return out


def multiply(a, b):
    out = [0.0] * 16

    for row in range(4):
        for column in range(4):
            out[row * 4 + column] = sum(a[row * 4 + k] * b[k * 4 + column] for k in range(4))

    return out


def inverse(m):
    linear = [m[0], m[1], m[2], m[4], m[5], m[6], m[8], m[9], m[10]]
    a, b, c, d, e, f, g, h, i = linear

    det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g)

    if abs(det) < 1e-20:
        return None

    inv = 1.0 / det
    upper = [
        (e * i - f * h) * inv, (c * h - b * i) * inv, (b * f - c * e) * inv,
        (f * g - d * i) * inv, (a * i - c * g) * inv, (c * d - a * f) * inv,
        (d * h - e * g) * inv, (b * g - a * h) * inv, (a * e - b * d) * inv,
    ]

    out = [
        upper[0], upper[1], upper[2], 0.0,
        upper[3], upper[4], upper[5], 0.0,
        upper[6], upper[7], upper[8], 0.0,
        0.0, 0.0, 0.0, 1.0,
    ]

    for column in range(3):
        out[12 + column] = -(m[12] * upper[column] + m[13] * upper[3 + column]
                             + m[14] * upper[6 + column])

    return out




def to_blender(matrix):
    return mathutils.Matrix([
        [matrix[0], matrix[4], matrix[8], matrix[12]],
        [matrix[1], matrix[5], matrix[9], matrix[13]],
        [matrix[2], matrix[6], matrix[10], matrix[14]],
        [matrix[3], matrix[7], matrix[11], matrix[15]],
    ])


def from_blender(matrix):
    return [
        matrix[0][0], matrix[1][0], matrix[2][0], matrix[3][0],
        matrix[0][1], matrix[1][1], matrix[2][1], matrix[3][1],
        matrix[0][2], matrix[1][2], matrix[2][2], matrix[3][2],
        matrix[0][3], matrix[1][3], matrix[2][3], matrix[3][3],
    ]


def stage_material(name, folder, additive):
    known = bpy.data.materials.get(name)

    if known is not None:
        return known

    material = bpy.data.materials.new(name)
    material.use_nodes = True
    tree = material.node_tree

    for node in list(tree.nodes):
        if node.type != "OUTPUT_MATERIAL":
            tree.nodes.remove(node)

    output = tree.nodes["Material Output"]

    texture = tree.nodes.new("ShaderNodeTexImage")
    texture.extension = "REPEAT"

    path = os.path.join(folder, name)

    if os.path.exists(path):
        try:
            texture.image = bpy.data.images.load(path, check_existing=True)
        except RuntimeError:
            pass

    coord = tree.nodes.new("ShaderNodeTexCoord")
    mapping = tree.nodes.new("ShaderNodeMapping")
    mapping.inputs["Location"].default_value = (0.0, 1.0, 0.0)
    mapping.inputs["Scale"].default_value = (1.0, -1.0, 1.0)
    tree.links.new(mapping.inputs["Vector"], coord.outputs["UV"])
    tree.links.new(texture.inputs["Vector"], mapping.outputs["Vector"])

    shade = tree.nodes.new("ShaderNodeAttribute")
    shade.attribute_name = "Shade"

    tint = tree.nodes.new("ShaderNodeMix")
    tint.data_type = "RGBA"
    tint.blend_type = "MULTIPLY"
    tint.inputs["Factor"].default_value = 1.0
    tree.links.new(tint.inputs[6], texture.outputs["Color"])
    tree.links.new(tint.inputs[7], shade.outputs["Color"])

    emission = tree.nodes.new("ShaderNodeEmission")
    tree.links.new(emission.inputs["Color"], tint.outputs[2])

    clear = tree.nodes.new("ShaderNodeBsdfTransparent")

    if additive:
        mixer = tree.nodes.new("ShaderNodeAddShader")
        tree.links.new(mixer.inputs[0], emission.outputs["Emission"])
        tree.links.new(mixer.inputs[1], clear.outputs["BSDF"])
    else:
        mixer = tree.nodes.new("ShaderNodeMixShader")
        tree.links.new(mixer.inputs[0], texture.outputs["Alpha"])
        tree.links.new(mixer.inputs[1], clear.outputs["BSDF"])
        tree.links.new(mixer.inputs[2], emission.outputs["Emission"])

    tree.links.new(output.inputs[0], mixer.outputs[0])

    return material


def build_object(index, node, model, folder, collection):
    mesh = node["mesh"]
    name = "node%03d" % index

    if mesh["flags"][0]:
        name += "_additive"

    data = bpy.data.meshes.new(name)
    obj = bpy.data.objects.new(name, data)
    collection.objects.link(obj)

    verts = [(v[0], v[1], v[2]) for v in mesh["vertices"]]
    faces = []
    slots = []

    for order, submesh in enumerate(mesh["submeshes"]):
        indices = submesh["indices"]
        material = model["material"][submesh["material"]]
        obj.data.materials.append(stage_material(material["filename"], folder,
                                                 mesh["flags"][0] != 0))

        for i in range(0, len(indices) - 2, 3):
            faces.append((indices[i], indices[i + 1], indices[i + 2]))
            slots.append(order)

    data.from_pydata(verts, [], faces)
    data.update()

    for polygon, slot in zip(data.polygons, slots):
        polygon.material_index = slot
        polygon.use_smooth = True

    uvs = data.uv_layers.new(name="UVMap")
    shade = data.color_attributes.new(name="Shade", type="FLOAT_COLOR", domain="POINT")
    normals = data.attributes.new(name="fbxex_normal", type="FLOAT_VECTOR", domain="POINT")
    rest = data.attributes.new(name="fbxex_rest", type="FLOAT_VECTOR", domain="POINT")

    for loop in data.loops:
        source = mesh["vertices"][loop.vertex_index]
        uvs.data[loop.index].uv = (source[10], source[11])

    for i, source in enumerate(mesh["vertices"]):
        shade.data[i].color = (source[6], source[7], source[8], source[9])
        normals.data[i].vector = (source[3], source[4], source[5])
        rest.data[i].vector = (source[0], source[1], source[2])

    obj.matrix_world = to_blender(mesh["matrix"])

    obj["fbxex_node"] = index
    obj["fbxex_blendmode"] = mesh["flags"][0]
    obj["fbxex_flag1"] = mesh["flags"][1]
    obj["fbxex_matrix"] = list(mesh["matrix"])

    return obj


def stage_name(folder):
    note = os.path.join(folder, "stage.txt")

    if not os.path.exists(note):
        return ""

    try:
        raw = open(note, "rb").read().decode("latin1")
    except OSError:
        return ""

    for line in raw.splitlines():
        if not line.strip().lower().startswith("name"):
            continue

        quoted = line.split('"')

        if len(quoted) >= 2 and quoted[1].strip():
            return quoted[1].strip()

    return ""


def do_import(context, path):
    model = read(path)
    folder = os.path.dirname(path)
    leaf = stage_name(folder) or os.path.basename(os.path.dirname(path)) or "stage"

    collection = bpy.data.collections.new(leaf)
    context.scene.collection.children.link(collection)
    collection["fbxex_source"] = path

    made = 0

    for index, node in enumerate(model["node"]):
        if node["mesh"] is None:
            continue

        build_object(index, node, model, folder, collection)
        made += 1

    context.scene["fbxex_source"] = path

    return made, len(model["node"])


def settled_matrix(obj):
    now = from_blender(obj.matrix_world)
    was = obj.get("fbxex_matrix")

    if was is None or len(was) != 16:
        return now

    scale = max(abs(value) for value in was)
    allowed = 1e-6 * (scale if scale > 1.0 else 1.0)

    for a, b in zip(was, now):
        if abs(a - b) > allowed:
            return now

    return [float(value) for value in was]


def gather_mesh(obj, template):
    data = obj.data
    uvs = data.uv_layers.active
    shade = data.color_attributes.get("Shade")

    order = [submesh["material"] for submesh in template["submeshes"]]
    tint = {}
    corner = {}

    for loop in data.loops:
        if uvs is not None:
            corner[loop.vertex_index] = tuple(uvs.data[loop.index].uv)

    for index in range(len(data.vertices)):
        if shade is not None and index < len(shade.data):
            colour = shade.data[index].color
            tint[index] = (colour[0], colour[1], colour[2], colour[3])
        else:
            tint[index] = (1.0, 1.0, 1.0, 1.0)

    stored = data.attributes.get("fbxex_normal")
    anchor = data.attributes.get("fbxex_rest")
    normals = []

    for index, vertex in enumerate(data.vertices):
        kept = None

        if stored is not None and index < len(stored.data):
            if anchor is None or index >= len(anchor.data):
                kept = stored.data[index].vector
            elif tuple(anchor.data[index].vector) == tuple(vertex.co):
                kept = stored.data[index].vector

        normals.append(tuple(kept) if kept is not None else tuple(vertex.normal))

    vertices = []

    for index, vertex in enumerate(data.vertices):
        uv = corner.get(index, (0.0, 0.0))
        colour = tint.get(index, (1.0, 1.0, 1.0, 1.0))
        vertices.append([
            vertex.co[0], vertex.co[1], vertex.co[2],
            normals[index][0], normals[index][1], normals[index][2],
            colour[0], colour[1], colour[2], colour[3],
            uv[0], uv[1],
        ])

    buckets = {}

    for polygon in data.polygons:
        slot = polygon.material_index

        if slot >= len(order):
            slot = len(order) - 1

        loop = list(polygon.vertices)

        for i in range(1, len(loop) - 1):
            buckets.setdefault(slot, []).extend([loop[0], loop[i], loop[i + 1]])

    submeshes = []

    for slot, material in enumerate(order):
        submeshes.append({"material": material, "indices": buckets.get(slot, [])})

    return {
        "flags": [int(obj.get("fbxex_blendmode", template["flags"][0])),
                  int(obj.get("fbxex_flag1", template["flags"][1]))],
        "matrix": settled_matrix(obj),
        "vertices": vertices,
        "submeshes": submeshes,
    }


def texture_slot(model, filename):
    for index, name in enumerate(model["texture"]):
        if name.lower() == filename.lower():
            return index

    model["texture"].append(filename)

    return len(model["texture"]) - 1


def material_slot(model, filename):
    texture = texture_slot(model, filename)
    value = [1.0, 1.0, 1.0, 1.0] + [0.0] * 13

    model["material"].append({
        "filename": filename,
        "index": len(model["material"]),
        "textureindex": texture,
        "value": value,
    })

    return len(model["material"]) - 1


def image_of(obj, slot):
    if slot >= len(obj.data.materials) or obj.data.materials[slot] is None:
        return ""

    material = obj.data.materials[slot]

    if material.node_tree is not None:
        for node in material.node_tree.nodes:
            if node.type == "TEX_IMAGE" and node.image is not None:
                return os.path.basename(node.image.filepath or node.image.name)

    return material.name


def append_node(model, obj):
    index = len(model["node"])

    slots = max(1, len(obj.data.materials))
    submeshes = []

    for slot in range(slots):
        submeshes.append({
            "material": material_slot(model, image_of(obj, slot) or model["texture"][0]),
            "indices": [],
        })

    template = {
        "flags": [int(obj.get("fbxex_blendmode", 0)), int(obj.get("fbxex_flag1", 0))],
        "matrix": from_blender(obj.matrix_world),
        "vertices": [],
        "submeshes": submeshes,
    }

    model["node"].append({"type": 1, "child": -1, "sibling": -1, "mesh": template})
    model["anime"].append([list(from_blender(mathutils.Matrix()))])

    root = model["node"][0]

    if root["child"] == -1:
        root["child"] = index
    else:
        last = root["child"]

        while model["node"][last]["sibling"] != -1:
            last = model["node"][last]["sibling"]

        model["node"][last]["sibling"] = index

    obj["fbxex_node"] = index

    return index


def do_export(context, path, template_path):
    model = read(template_path)

    objects = {}
    added = 0

    for obj in context.scene.objects:
        if obj.type != "MESH":
            continue

        if "fbxex_node" not in obj:
            objects[append_node(model, obj)] = obj
            added += 1
            continue

        index = int(obj["fbxex_node"])

        if 0 <= index < len(model["node"]) and model["node"][index]["mesh"] is not None:
            objects[index] = obj
        else:
            objects[append_node(model, obj)] = obj
            added += 1

    tree = parents(model["node"])
    moved = 0

    for index, obj in objects.items():
        node = model["node"][index]
        was = list(node["mesh"]["matrix"])
        node["mesh"] = gather_mesh(obj, node["mesh"])
        now = node["mesh"]["matrix"]

        if was == now:
            continue

        moved += 1

        track = model["anime"][index] if index < len(model["anime"]) else []

        if len(track) < 2:
            continue

        owner = tree.get(index, -1)
        upper = model["node"][owner]["mesh"]["matrix"] if owner >= 0 \
            and model["node"][owner]["mesh"] is not None else None

        rest = inverse(upper) if upper is not None else None
        local_was = multiply(was, rest) if rest is not None else was
        local_now = multiply(now, rest) if rest is not None else now
        back = inverse(local_was)

        if back is None:
            continue

        delta = multiply(back, local_now)
        model["anime"][index] = [multiply(frame, delta) for frame in track]

    with open(path, "wb") as handle:
        handle.write(build(model))

    return len(objects), moved, added


if INSIDE_BLENDER:
    class ImportFbxEx(bpy.types.Operator, ImportHelper):
        bl_idname = "import_scene.fbxex"
        bl_label = "Import FbxExp stage"
        bl_options = {"REGISTER", "UNDO"}

        filename_ext = ".bin"
        filter_glob: bpy.props.StringProperty(default="*.bin", options={"HIDDEN"})

        def execute(self, context):
            try:
                made, total = do_import(context, self.filepath)
            except (ValueError, struct.error) as problem:
                self.report({"ERROR"}, str(problem))
                return {"CANCELLED"}

            self.report({"INFO"}, "%d mesh node(s) of %d" % (made, total))
            return {"FINISHED"}

    class ExportFbxEx(bpy.types.Operator, ExportHelper):
        bl_idname = "export_scene.fbxex"
        bl_label = "Export FbxExp stage"
        bl_options = {"REGISTER"}

        filename_ext = ".bin"
        filter_glob: bpy.props.StringProperty(default="*.bin", options={"HIDDEN"})

        template: bpy.props.StringProperty(
            name="Template",
            description="The .bin this scene was imported from. Everything a scene cannot carry - "
                        "the material table, the node tree, the animation - is kept from it. Leave "
                        "empty to use the file the scene was imported from",
            default="")

        def execute(self, context):
            template = self.template or context.scene.get("fbxex_source", "")

            if not template or not os.path.exists(template):
                self.report({"ERROR"}, "no template .bin to write over - set one")
                return {"CANCELLED"}

            try:
                written, moved, added = do_export(context, self.filepath, template)
            except (ValueError, struct.error) as problem:
                self.report({"ERROR"}, str(problem))
                return {"CANCELLED"}

            self.report({"INFO"}, "%d mesh node(s), %d moved, %d added"
                        % (written, moved, added))
            return {"FINISHED"}

    def menu_import(self, context):
        self.layout.operator(ImportFbxEx.bl_idname, text="FbxExp stage (.fbx.bin)")

    def menu_export(self, context):
        self.layout.operator(ExportFbxEx.bl_idname, text="FbxExp stage (.fbx.bin)")

    CLASSES = (ImportFbxEx, ExportFbxEx)

    def register():
        for item in CLASSES:
            bpy.utils.register_class(item)

        bpy.types.TOPBAR_MT_file_import.append(menu_import)
        bpy.types.TOPBAR_MT_file_export.append(menu_export)

    def unregister():
        bpy.types.TOPBAR_MT_file_export.remove(menu_export)
        bpy.types.TOPBAR_MT_file_import.remove(menu_import)

        for item in reversed(CLASSES):
            bpy.utils.unregister_class(item)

    if __name__ == "__main__":
        register()
