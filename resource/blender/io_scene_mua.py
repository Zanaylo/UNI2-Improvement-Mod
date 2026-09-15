#OBS: Não sei se vou fazer manutenção dessa poha aqui. o importer para o UNI2 ainda está fudido mas agradeço por ler ou traduzir essa mensagem :)

"""Read AWS `MUA` model that is on bobleis games.
mot -> stage character or even effect -> skeleton, mashes 
(ca) mmot -> model own bones tracks, uv scroll
evb -> scripts
"""

bl_info = {
    "name": "Mua model (.mua)",
    "author": "PrimoZanaylo",
    "version": (0, 5, 0),
    "blender": (4, 2, 0),
    "location": "File > Import > Mua model",
    "description": "Import a Mua model with its animation and scripts",
    "category": "Import-Export",
}

import hashlib
import math
import os
import struct
import zlib

import bpy
import mathutils
from bpy.props import BoolProperty, EnumProperty, FloatProperty, StringProperty
from bpy_extras.io_utils import ExportHelper, ImportHelper

#just some eye balling stuff ignore it
CHARACTER = 213.0 
UNI2_CHARACTER = 0.132
FPS = 60

#Noesis information
PAC_HEADER = 0x20
VERTEX = 0x50
SECTIONS = 17

#.Mua
SKELETON, BONE, MESH, PART, MATERIAL, ASSIGN, TEXTURE = 0, 1, 2, 3, 4, 5, 6
UVANIM, TRACKLIST, TRACKKEY = 7, 8, 9
SCRIPT, MESHPART, VERTEX_SECTION, INDEX, STRING_INFO, STRING = 11, 12, 13, 14, 15, 16

TRANSLATION, ROTATION, SHEAR, SCALE = 0x04, 0x14, 0x24, 0x34
KINDS = (TRANSLATION, ROTATION, SHEAR, SCALE)
WIDTH = {TRANSLATION: 3, ROTATION: 3, SHEAR: 4, SCALE: 3}

#.MMOT
MOTION_SECTIONS = 11

MOTION_BONES, MOTION_LIST, MOTION_KEYS, MOTION_INFO, MOTION_STRINGS = 1, 2, 3, 8, 9

EVB_RECORD = 0x20
EVB_BLOCKS = 0x30
EVB_OPEN, EVB_YIELD, EVB_TIME, EVB_CLOSE, EVB_GROUP = 0x01, 0x02, 0x03, 0x04, 0x05
EVB_MOTION, EVB_BEGIN, EVB_END, EVB_RECT, EVB_LOOP, EVB_RAMP = 0x06, 0x09, 0x0a, 0x0b, 0x0f, 0x12
EVB_FRAMES = 12000
EVB_NONE = 0xffffffff

KEY = bytes((
    0xf5, 0x5c, 0x84, 0x2a, 0xad, 0x61, 0x54, 0xe7, 0x0a, 0xfc, 0x99, 0x6b, 0xd5, 0xa4, 0xd3, 0xd8,
    0x48, 0x26, 0x69, 0xcb, 0x07, 0x42, 0x13, 0x5e, 0x10, 0x23, 0xd2, 0x6d, 0x36, 0xc7, 0xc1, 0x66,
    0xdf, 0xa1, 0xad, 0xf1, 0x44, 0x44, 0x7e, 0xc9, 0x8e, 0x24, 0x99,
))

MAGIC = (b"FPAC", b"MUA\x00", b"MMOT", b"EVT0", b"DDS ", b"DFAS")
PACKED = b"DFAS"

IDENTITY = [1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0]

SOFT_LOW = 16
SOFT_HIGH = 239
SOFT_SHARE = 0.5
GLOW_SOLID = 0.01
GLOW_SOFT = 0.05
GREY_SPREAD = 6.0
MASK_LEVEL = 0.15
BACK_CLEAR = 8
BACK_SHARE = 0.05
BACK_LOW = 8.0
BACK_HIGH = 64.0
ADDS_MOST = 128
ADDS_SPAN = 2.0
WINDOW_COVER = 0.9
WINDOW_BAND = 0.6
WINDOW_MATCH = 0.25
WINDOW_BANDS = 8


def dword(blob, at):
    return struct.unpack_from("<I", blob, at)[0]


def decrypted(name, blob):
    if len(blob) < 4 or blob[:4] in MAGIC:
        return blob

    start = hashlib.md5(name.lower().encode("utf-8")).digest()[7] % len(KEY)
    pad = KEY[start:] + KEY * (len(blob) // len(KEY) + 1)
    mixed = int.from_bytes(blob, "big") ^ int.from_bytes(pad[:len(blob)], "big")

    return mixed.to_bytes(len(blob), "big")


def unpacked(blob):
    #DFAS CF parte 
    if len(blob) < 16 or blob[:4] != PACKED:
        return blob

    try:
        return zlib.decompress(blob[16:])
    except zlib.error:
        return blob


def is_archive(blob):
    return len(blob) >= PAC_HEADER and blob[:4] == b"FPAC"


def pac_entries(blob):
    start, _total, count, _one, name_bytes = struct.unpack_from("<5I", blob, 4)

    if count == 0 or name_bytes == 0:
        return []

    stride = (name_bytes + 12 + 15) // 16 * 16

    if start > PAC_HEADER:
        stated = (start - PAC_HEADER) // count

        if stated >= name_bytes + 12:
            stride = stated

    out = []

    for i in range(count):
        at = PAC_HEADER + i * stride

        if at + name_bytes + 12 > len(blob):
            break

        name = blob[at:at + name_bytes].split(b"\x00")[0].decode("shift_jis", "replace")
        _index, offset, size = struct.unpack_from("<3I", blob, at + name_bytes)

        if start + offset + size > len(blob):
            break

        out.append((name, start + offset, size))

    return out


def pac_walk(blob, prefix=""):
    out = {}

    for name, at, size in pac_entries(blob):
        body = unpacked(blob[at:at + size])

        if is_archive(body):
            out.update(pac_walk(body, prefix + name.rsplit(".", 1)[0] + "/"))
            continue

        out[prefix + name] = body

    return out


def written_pac(blob, swap):
    start, total, count, one, name_bytes = struct.unpack_from("<5I", blob, 4)
    stride = (start - PAC_HEADER) // count if count else 0
    head = bytearray(blob[:start])
    body = bytearray()

    for i in range(count):
        at = PAC_HEADER + i * stride
        name = blob[at:at + name_bytes].split(b"\x00")[0].decode("shift_jis", "replace")
        index, offset, size = struct.unpack_from("<3I", blob, at + name_bytes)
        content = swap.get(name, blob[start + offset:start + offset + size])
        struct.pack_into("<3I", head, at + name_bytes, index, len(body), len(content))
        body.extend(content)
        body.extend(b"\x00" * (-len(body) % 16))

    struct.pack_into("<I", head, 8, start + len(body))
    out = bytes(head) + bytes(body)

    return out + blob[total:] if len(blob) > total else out


def opened(path):
    with open(path, "rb") as handle:
        blob = unpacked(decrypted(os.path.basename(path), handle.read()))

    if not is_archive(blob):
        return {os.path.basename(path): blob}

    return pac_walk(blob)


def multiply(a, b):
    out = [0.0] * 16

    for r in range(4):
        for c in range(4):
            out[r * 4 + c] = sum(a[r * 4 + k] * b[k * 4 + c] for k in range(4))

    return out


def rotation(axis, angle):
    turn = mathutils.Matrix.Rotation(angle, 4, "XYZ"[axis])

    return [turn[0][0], turn[1][0], turn[2][0], 0.0,
            turn[0][1], turn[1][1], turn[2][1], 0.0,
            turn[0][2], turn[1][2], turn[2][2], 0.0,
            0.0, 0.0, 0.0, 1.0]


def compose(translate, rotate, size):
    out = [size[0], 0.0, 0.0, 0.0, 0.0, size[1], 0.0, 0.0, 0.0, 0.0, size[2], 0.0,
           0.0, 0.0, 0.0, 1.0]

    for axis in (1, 0, 2):
        out = multiply(out, rotation(axis, rotate[axis]))

    out[12] = translate[0]
    out[13] = translate[1]
    out[14] = translate[2]

    return out


def as_matrix(row):
    return mathutils.Matrix((
        (row[0], row[4], row[8], row[12]),
        (row[1], row[5], row[9], row[13]),
        (row[2], row[6], row[10], row[14]),
        (row[3], row[7], row[11], row[15]),
    ))


class Track(object):
    def __init__(self):
        self.key = {}

    def value(self, frame, fallback):
        if not self.key:
            return fallback

        exact = self.key.get(frame)

        if exact is not None:
            return exact

        below = [f for f in self.key if f <= frame]
        above = [f for f in self.key if f > frame]

        if not below:
            return self.key[min(self.key)]

        if not above:
            return self.key[max(below)]

        low, high = max(below), min(above)
        across = (frame - low) / float(high - low)
        a, b = self.key[low], self.key[high]

        return tuple(a[i] + (b[i] - a[i]) * across for i in range(len(a)))

    def varies(self):
        return len(set(tuple(v) for v in self.key.values())) > 1


class Tracks(object):
    #Four tracks per bone(to do?) in the one order both containers store them in

    def __init__(self, bones):
        self.bones = bones
        self.frames = 0
        self.track = [dict((kind, Track()) for kind in KINDS) for _ in range(bones)]

    def add(self, bone, kind, value, frame):
        self.track[bone][kind].key[frame] = value
        self.frames = max(self.frames, frame + 1)

    def sample(self, bone, frame, rest):
        own = self.track[bone]

        return (own[TRANSLATION].value(frame, rest[0]),
                own[ROTATION].value(frame, rest[1]),
                own[SCALE].value(frame, rest[2]))

    def moves(self, bone):
        return any(self.track[bone][kind].varies() for kind in (TRANSLATION, ROTATION, SCALE))

    def keyframes(self, bone):
        out = set()

        for kind in (TRANSLATION, ROTATION, SCALE):
            out.update(self.track[bone][kind].key)

        return out

#Remodular
class Motion(Tracks):
    def __init__(self, blob):
        if blob[:4] != b"MMOT":
            raise ValueError("not an MMOT motion")

        section = [struct.unpack_from("<2I", blob, 0x20 + i * 0x10)
                   for i in range(MOTION_SECTIONS)]
        Tracks.__init__(self, section[MOTION_BONES][1])

        self.string = strings(blob, section[MOTION_INFO], section[MOTION_STRINGS][0])
        at = section[MOTION_KEYS][0]
        listed = section[MOTION_LIST][0]

        for bone in range(self.bones):
            for kind in KINDS:
                for _ in range(dword(blob, listed + bone * 0x40 + kind)):
                    value = struct.unpack_from("<%df" % WIDTH[kind], blob, at)
                    self.add(bone, kind, value, dword(blob, at + 0x10))
                    at += 0x20

    def name(self):
        return self.string[0] if self.string else ""

    def target(self):
        return self.string[1] if len(self.string) > 1 else ""

#...
class Take(Tracks):
    #Sec 8 and 9 seems to be like mmot but in mua?

    def __init__(self, model, first, count):
        Tracks.__init__(self, count)

        for i in range(count):
            bone = model.bone[first + i]

            if bone["frames"] < 2:
                continue

            for kind, index in zip(KINDS, bone["track"]):
                for value, frame in model.keys(index):
                    self.add(i, kind, value[:WIDTH[kind]], frame)

            self.frames = max(self.frames, bone["frames"])


def strings(blob, info, base):
    at, count = info
    out = []

    for i in range(count):
        offset, length = struct.unpack_from("<2I", blob, at + i * 0x10)
        raw = blob[base + offset:base + offset + length]
        out.append(raw.split(b"\x00")[0].decode("shift_jis", "replace"))

    return out

#Thanks a lot Noesis goat
class Model(object):
    def __init__(self, blob):
        if blob[:4] != b"MUA\x00":
            raise ValueError("not a MUA model")

        self.blob = blob
        self.section = [struct.unpack_from("<2I", blob, 0x20 + i * 8) for i in range(SECTIONS)]

        self.string = strings(blob, self.section[STRING_INFO], self.section[STRING][0])
        self._textures()
        self._materials()
        self._bones()
        self._meshes()

    def _at(self, section, index, stride):
        return self.section[section][0] + index * stride

    def _count(self, section):
        return self.section[section][1]

    def _name(self, index):
        return self.string[index] if 0 <= index < len(self.string) else "?%d" % index

    def _textures(self):
        self.texture = [self._name(dword(self.blob, self._at(TEXTURE, i, 0x10)))
                        for i in range(self._count(TEXTURE))]

    def _materials(self):
        assign = []
        self.flow = []
        self.value = []

        for i in range(self._count(ASSIGN)):
            at = self._at(ASSIGN, i, 0x20)
            assign.append(struct.unpack_from("<2i", self.blob, at)[1])
            count, first = struct.unpack_from("<2i", self.blob, at + 8)
            self.flow.append([self._uvkey(first + k) for k in range(count)
                              if 0 <= first + k < self._count(UVANIM)])

        self.material = []
        self.materialFlow = []

        for i in range(self._count(MATERIAL)):
            at = self._at(MATERIAL, i, 0x50)
            count, first = struct.unpack_from("<2i", self.blob, at)
            taken = [first + k for k in range(count) if 0 <= first + k < len(assign)]
            self.material.append([assign[k] for k in taken])
            self.materialFlow.append(self.flow[taken[0]] if taken else [])
            self.value.append(list(struct.unpack_from("<18f", self.blob, at + 8)))

    def _uvkey(self, index):
        at = self._at(UVANIM, index, 0x1c)
        value = struct.unpack_from("<4f", self.blob, at)

        return {"offset": value[:2], "scale": value[2:], "frame": dword(self.blob, at + 0x10)}

    def _bones(self):
        self.bone = []

        for i in range(self._count(BONE)):
            at = self._at(BONE, i, 0x130)
            self.bone.append({
                "name": self._name(dword(self.blob, at)),
                "parent": struct.unpack_from("<i", self.blob, at + 0x3c)[0],
                "matrix": list(struct.unpack_from("<16f", self.blob, at + 0x48)),
                "rest": (struct.unpack_from("<3f", self.blob, at + 8),
                         struct.unpack_from("<3f", self.blob, at + 20),
                         struct.unpack_from("<3f", self.blob, at + 32)),
                "frames": dword(self.blob, at + 0x108),
                "track": list(struct.unpack_from("<4i", self.blob, at + 0x10c)),
            })

        self.skeleton = []
        self.skeletonScript = []

        for i in range(self._count(SKELETON)):
            at = self._at(SKELETON, i, 0x20)
            self.skeleton.append(struct.unpack_from("<2I", self.blob, at))
            self.skeletonScript.append(struct.unpack_from("<i", self.blob, at + 8)[0])

        self.script = [self._name(dword(self.blob, self._at(SCRIPT, i, 0x10)))
                       for i in range(self._count(SCRIPT))]

        self.owner = {}

        for index, (first, count) in enumerate(self.skeleton):
            for i in range(count):
                self.owner.setdefault(first + i, (index, first, i))

    def _meshes(self):
        self.part = []

        for i in range(self._count(PART)):
            material, indices, first = struct.unpack_from("<3i", self.blob,
                                                          self._at(PART, i, 0x20))
            self.part.append({"material": material, "indices": indices, "first": first})

        self.mesh = []

        for i in range(self._count(MESH)):
            at = self._at(MESH, i, 0xc0)
            parts, first_part, vertices, first_vertex = struct.unpack_from("<4I", self.blob, at + 8)
            index = int(struct.unpack_from("<f", self.blob, at)[0])
            self.mesh.append({
                "name": self._name(struct.unpack_from("<i", self.blob, at + 0xb4)[0]),
                "skeleton": index,
                "bone": self.skeleton[index][0] if 0 <= index < len(self.skeleton) else -1,
                "bones": self.skeleton[index][1] if 0 <= index < len(self.skeleton) else 0,
                "parts": parts, "firstPart": first_part,
                "vertices": vertices, "firstVertex": first_vertex,
            })

    def vertex(self, index):
        at = self.section[VERTEX_SECTION][0] + index * VERTEX
        colour = struct.unpack_from("<4B", self.blob, at + 0x34)
        bones = struct.unpack_from("<3f", self.blob, at + 0x38)
        weights = struct.unpack_from("<3f", self.blob, at + 0x44)

        return {
            "position": struct.unpack_from("<3f", self.blob, at),
            "normal": struct.unpack_from("<3f", self.blob, at + 0x0c),
            "uv": struct.unpack_from("<2f", self.blob, at + 0x24),
            "colour": (colour[2], colour[1], colour[0], colour[3]),
            "weights": [(int(bones[k]), weights[k]) for k in range(3)
                        if int(bones[k]) >= 0 and weights[k] > 0.0],
        }

    def strip(self, part):
        if part["indices"] < 1:
            return ()

        at = self.section[INDEX][0] + part["first"] * 2

        return struct.unpack_from("<%dH" % part["indices"], self.blob, at)

    def triangles(self, part):
        if part["indices"] < 3:
            return []

        strip = self.strip(part)
        out = []

        for i in range(len(strip) - 2):
            a, b, c = strip[i], strip[i + 1], strip[i + 2]

            if a == b or b == c or a == c:
                continue

            out.append((a, c, b) if i % 2 else (a, b, c))

        return out

    def keys(self, track):
        if not 0 <= track < self._count(TRACKLIST):
            return []

        at, count = struct.unpack_from("<2I", self.blob, self._at(TRACKLIST, track, 0x10))
        out = []

        for i in range(count):
            row = self._at(TRACKKEY, at + i, 0x20)
            out.append((struct.unpack_from("<4f", self.blob, row), dword(self.blob, row + 0x10)))

        return out

    def chain(self, first, local, matrices):
        out = None
        seen = set()

        while 0 <= local < len(matrices) and local not in seen:
            seen.add(local)
            out = matrices[local] if out is None else multiply(out, matrices[local])
            local = self.bone[first + local]["parent"]

        return out or list(IDENTITY)

    def world(self, index):
        if index not in self.owner:
            return list(self.bone[index]["matrix"]) if 0 <= index < len(self.bone) \
                else list(IDENTITY)

        _which, first, local = self.owner[index]
        count = self.skeleton[_which][1]

        return self.chain(first, local, [self.bone[first + i]["matrix"] for i in range(count)])

    def scriptOf(self, mesh):
        which = mesh["skeleton"]

        if not 0 <= which < len(self.skeletonScript):
            return ""

        index = self.skeletonScript[which]

        if not 0 <= index < len(self.script):
            return ""

        named = self.script[index]

        return named.rsplit(".", 1)[0]
        
#Todo: optimize the load script processing + 


class Script(object):
    #EVT0 7 blocks 0x20, Names in 0x22, records off 0x10

    def __init__(self, blob):
        if blob[:4] != b"EVT0":
            raise ValueError("not an EVT0 script")

        _version, _names, self.last, commands, stride = struct.unpack_from("<5I", blob, 4)
        skipped, count = struct.unpack_from("<2H", blob, 0x20)

        self.named = []
        at = EVB_BLOCKS + skipped * stride

        for _ in range(count):
            if at + stride > len(blob):
                break

            raw = blob[at:at + stride].split(b"\x00")[0]

            if not all(0x20 <= letter < 0x7f for letter in raw):
                break

            self.named.append(raw.decode("ascii"))
            at += stride

        self.record = [struct.unpack_from("<8I", blob, where)
                       for where in range(commands, len(blob) - EVB_RECORD + 1, EVB_RECORD)]

    def timeline(self, opcode):
        #'Usagi.evb' still broken
        #cmds in 0x03, 0x0f  script loop, 0x09 own clock -> ai ai viu
        out = []
        loop = 0
        time = 0

        for fields in self.record:
            code = fields[0]

            if code in (EVB_NONE, EVB_YIELD):
                break

            if code == EVB_TIME:
                time = fields[1]
            elif code == EVB_LOOP:
                loop = time
                break
            elif code == opcode:
                out.append((time,) + fields[1:])

        if loop > EVB_FRAMES:
            return 0, []

        return loop, out

    def picks(self):
        loop, entries = self.timeline(EVB_MOTION)

        if loop < 2:
            return 0, []

        out = []

        for fields in entries:
            index = fields[1]

            if 0 <= index < len(self.named):
                out.append((fields[0], self.named[index]))

        return (loop, out) if out else (0, [])

    def ramps(self):
        loop, entries = self.timeline(EVB_RAMP)

        if loop < 2 or not entries:
            return 0, []

        return loop, [(f[0], f[1], max(1, f[2])) for f in entries]

    def rects(self):
        #0x0b group -> 0x05 select: own f counts and cycling
        loop, entries = self.timeline(EVB_GROUP)

        if not entries:
            return 0, []

        groups = {}
        current = None


        for fields in self.record:
            code = fields[0]

            if code == EVB_NONE:
                break

            if code == EVB_BEGIN:
                current = fields[1]
                groups.setdefault(current, [])
            elif code == EVB_END:
                current = None
            elif code == EVB_RECT and current is not None:
                groups[current].append((fields[1], fields[2], fields[3], fields[4], fields[5],
                                        fields[6]))

        if loop < 2:
            spans = [sum(row[0] for row in held) for held in groups.values() if held]
            loop = max(spans) if spans else 0

        if loop < 2 or loop > EVB_FRAMES:
            return 0, []

        out = []

        for frame in range(loop):
            chosen = None
            began = 0

            for at, group in ((f[0], f[1]) for f in entries):
                if at <= frame:
                    chosen = group
                    began = at

            held = groups.get(chosen) or []
            span = sum(r[0] for r in held)

            if not held or span <= 0:
                out.append(None)
                continue

            step = (frame - began) % span

            for stay, sheet, x, y, w, h in held:
                if step < stay:
                    out.append((sheet, x, y, w, h))
                    break

                step -= stay

        return loop, out

    def listing(self):
        out = ["names: %s" % (", ".join(self.named) or "-"), ""]

        for fields in self.record:
            if fields[0] == EVB_NONE:
                out.append("end")
                break

            args = " ".join(("%d" % f) if f != EVB_NONE else "-" for f in fields[1:])
            out.append("op 0x%02x  %s" % (fields[0], args))

        return "\n".join(out) + "\n"

#oh formato lixo da desgraça
BIN_SECTIONS = 3
BIN_RECORD = 0x40
JONB_HEAD = 211
JONB_BOX = 20

def bin_strings(blob, info, count, base):
    out = []

    for i in range(count):
        offset, length = struct.unpack_from("<2I", blob, info + i * 0x10)
        raw = blob[base + offset:base + offset + length]
        out.append(raw.split(b"\x00")[0].decode("shift_jis", "replace"))

    return out


def bin_listing(blob):
    #WHY? cmds are named in here but not in....?????????????
    if blob[:4] != b"EVT\x00":
        raise ValueError("not an EVT script")

    sections = [struct.unpack_from("<2I", blob, 0x10 + i * 0x10) for i in range(BIN_SECTIONS)]
    strings = bin_strings(blob, sections[1][0], sections[1][1], sections[2][0])
    at, count = sections[0]
    out = ["strings: %s" % ", ".join(strings), ""]
    depth = ""

    for i in range(count):
        fields = struct.unpack_from("<16i", blob, at + i * BIN_RECORD)
        name = strings[fields[0]] if 0 <= fields[0] < len(strings) else "?%d" % fields[0]
        args = list(fields[1:])

        while args and args[-1] == 0:
            args.pop()

        shown = []

        for order, value in enumerate(args):
            if order == 0 and 0 < value < len(strings):
                shown.append("%d (%s)" % (value, strings[value]))
                continue

            shown.append(str(value))

        if name == "EventEnd":
            depth = depth[:-2]

        out.append("%s%-14s %s" % (depth, name, " ".join(shown)))

        if name == "EventHead":
            depth += "  "

    return "\n".join(out) + "\n"

#formato lixo 2
def jonbin_listing(blob):
    #211 bytes header -> +20 per box 4 floats (not in model unit?) needs to be verified again but works for now and seems to be just collision shit
    if blob[:4] != b"JONB":
        raise ValueError("not a JONB collision")

    drawn = blob[6:38].split(b"\x00")[0].decode("shift_jis", "replace")
    count = struct.unpack_from("<I", blob, 0x2d)[0]

    if JONB_HEAD + JONB_BOX * count != len(blob):
        raise ValueError("%d box(es) do not fill %d bytes" % (count, len(blob)))

    canvas = struct.unpack_from("<6f", blob, 0x8b)
    out = ["drawn over: %s" % drawn,
           "canvas: %g x %g, origin %g %g" % (canvas[0], canvas[1], canvas[2], canvas[3]),
           "%d box(es): kind, x, y, width, height" % count, ""]

    for i in range(count):
        at = JONB_HEAD + i * JONB_BOX
        kind = struct.unpack_from("<I", blob, at)[0]
        x, y, w, h = struct.unpack_from("<4f", blob, at + 4)
        out.append("%2d  kind %d  %8.2f %8.2f %8.2f %8.2f" % (i, kind, x, y, w, h))

    return "\n".join(out) + "\n"


def build_bins(files, label):
    #2 containers, just that
    made = 0

    for leaf in sorted(files):
        body = files[leaf]
        listing = None

        try:
            if body[:4] == b"EVT\x00":
                listing = bin_listing(body)
            elif body[:4] == b"JONB":
                listing = jonbin_listing(body)
        except (ValueError, struct.error, IndexError):
            listing = None

        if listing is None:
            continue

        text = bpy.data.texts.new("%s %s" % (label, os.path.basename(leaf)))
        text.write(listing)
        made += 1

    return made


def named_bones(model):
    out = {}

    for index, bone in enumerate(model.bone):
        out.setdefault(bone["name"], index)

    return out


def takes(model, files):
    #to do
    out = {}
    named = named_bones(model)

    for leaf in sorted(files):
        if not leaf.lower().endswith(".mmot"):
            continue

        try:
            motion = Motion(files[leaf])
        except (ValueError, struct.error):
            continue

        root = named.get(motion.target())

        if root is None or root not in model.owner or motion.frames < 2:
            continue

        _which, first, _local = model.owner[root]

        if first != root or model.skeleton[_which][1] != motion.bones:
            continue

        out.setdefault(first, {})[os.path.basename(leaf)] = motion

    for first, count in model.skeleton:
        take = Take(model, first, count)

        if take.frames > 1 and any(take.moves(i) for i in range(count)):
            out.setdefault(first, {})["(the model's own)"] = take

    return out


def scripts(files):
    out = {}

    for leaf, body in files.items():
        if not leaf.lower().endswith(".evb"):
            continue

        try:
            out[os.path.basename(leaf)[:-4]] = Script(body)
        except (ValueError, struct.error):
            continue

    return out


def stage_of(path):
    leaf = os.path.basename(path)

    for tail in ("_vtx.pac", "_img.pac", ".pac"):
        if leaf.lower().endswith(tail):
            return leaf[:-len(tail)]

    return os.path.splitext(leaf)[0]


def unpacked_into(art, folder):
    #Needs a better way of doing this shit
    if os.path.isdir(folder) and any(leaf.lower().endswith(".dds") for leaf in os.listdir(folder)):
        return folder

    found = [(name, body) for name, body in opened(art).items() if body[:4] == b"DDS "]

    if not found:
        return ""

    if not os.path.isdir(folder):
        os.makedirs(folder)

    for name, body in found:
        where = os.path.join(folder, os.path.basename(name))

        if not os.path.exists(where):
            with open(where, "wb") as out:
                out.write(body)

    return folder


def unpack_textures(where, stage):
    out = []

    for folder in where:
        if not os.path.isdir(folder):
            continue

        archives = [os.path.join(folder, leaf) for leaf in sorted(os.listdir(folder))
                    if leaf.lower().endswith(".pac")]
        named = [one for one in archives
                 if os.path.basename(one).lower() == stage.lower() + "_img.pac"]

        for art in (named or archives):
            into = unpacked_into(art, art[:-4])

            if into:
                out.append(into)

        out.append(folder)

    return [folder for i, folder in enumerate(out) if folder not in out[:i]]

#todo: arquivos em caixa baixa
def texture_index(folders):
    out = {}

    for folder in folders:
        if not os.path.isdir(folder):
            continue

        for leaf in sorted(os.listdir(folder)):
            where = os.path.join(folder, leaf)

            if leaf.lower() not in out and os.path.isfile(where):
                out[leaf.lower()] = where

    return out


def sheet_sizes(index):
    out = {}

    for leaf, where in index.items():
        with open(where, "rb") as handle:
            head = handle.read(20)

        if len(head) < 20 or head[:4] != b"DDS ":
            continue

        height, width = struct.unpack_from("<2I", head, 12)
        out[leaf] = (width, height)

    return out


def found_texture(index, leaf):
    if not leaf:
        return ""

    stem, kind = os.path.splitext(leaf)

    for name in (leaf, stem + "_0" + kind):
        where = index.get(name.lower())

        if where:
            return where

    return ""


def with_geometry(files):
    out = []

    for name in sorted(files):
        if not name.lower().endswith(".mua"):
            continue

        found = Model(files[name])

        if found.section[VERTEX_SECTION][1]:
            out.append((name, found))

    return out


def gathered(path):
    #working for now, may break later
    files = opened(path)
    models = with_geometry(files)
    origin = path if models else ""
    folder = os.path.dirname(path)
    stage = stage_of(path)
    beside = {}

    for leaf in sorted(os.listdir(folder)):
        where = os.path.join(folder, leaf)

        if not os.path.isfile(where) or os.path.samefile(where, path):
            continue

        if leaf.lower().endswith((".mmot", ".evb")):
            with open(where, "rb") as handle:
                beside[leaf] = decrypted(leaf, handle.read())

            continue

        if not leaf.lower().endswith(".pac") or leaf.lower().endswith("_img.pac"):
            continue

        if stage_of(leaf) != stage:
            continue

        found = opened(where)

        for name, body in found.items():
            beside.setdefault(name, body)

        if not models:
            models = with_geometry(found)

            if models:
                origin = where

    if not models:
        raise ValueError("No .MUA with geometry in %s or next to it" % os.path.basename(path))

    files.update(beside)

    return models, files, origin


def unique(existing, name):
    if name not in existing:
        return name

    at = 1

    while "%s.%03d" % (name, at) in existing:
        at += 1

    return "%s.%03d" % (name, at)


def curve_of(action, owner, path, index):
    if hasattr(action, "fcurve_ensure_for_datablock"):
        return action.fcurve_ensure_for_datablock(owner, path, index=index)

    return action.fcurves.new(path, index=index)


def write_curve(action, owner, path, index, frames, values, kind="LINEAR"):
    fcurve = curve_of(action, owner, path, index)

    if len(fcurve.keyframe_points):
        fcurve.keyframe_points.clear()

    fcurve.keyframe_points.add(len(frames))
    flat = []

    for frame, value in zip(frames, values):
        flat.extend((float(frame), float(value)))

    fcurve.keyframe_points.foreach_set("co", flat)

    for point in fcurve.keyframe_points:
        point.interpolation = kind

    fcurve.update()

    return fcurve


def action_for(owner, name):
    owner.animation_data_create()

    if owner.animation_data.action is not None:
        return owner.animation_data.action

    action = bpy.data.actions.new(name)
    owner.animation_data.action = action

    return action


def fresh_action(owner, name):
    owner.animation_data_create()
    action = bpy.data.actions.new(name)
    owner.animation_data.action = action

    return action


def curves_of(action):
    if not hasattr(action, "fcurve_ensure_for_datablock"):
        return list(action.fcurves)

    if not len(action.layers) or not len(action.slots):
        return []

    return list(action.layers[0].strips[0].channelbag(action.slots[0]).fcurves)


def cycled(action):
    for fcurve in curves_of(action):
        if any(one.type == "CYCLES" for one in fcurve.modifiers):
            continue

        fcurve.modifiers.new("CYCLES")


def sheet_of(model, slot):
    assigned = model.material[slot] if 0 <= slot < len(model.material) else []

    if assigned and 0 <= assigned[0] < len(model.texture):
        return os.path.basename(model.texture[assigned[0]]).lower()

    return ""


def material_name(model, slot, prefix):
    return "%sm%03d %s" % (prefix + " " if prefix else "", slot, sheet_of(model, slot))

#thanks that blazblue and uni uses the same fucking thing
def dds_blocks(blob):
    if len(blob) < 148 or blob[:4] != b"DDS ":
        return (None, 0, 0, 0)

    fourcc = blob[84:88]
    body = blob[128:]

    if fourcc == b"DXT1":
        return (body, 8, 0, -1)

    if fourcc == b"DXT5":
        return (body, 16, 8, 0)

    if fourcc == b"DXT3":
        return (body, 16, 8, -1)

    return (None, 0, 0, 0)


def dds_levels(first, second):
    if first > second:
        return [first, second] + [((6 - k) * first + (1 + k) * second) // 7 for k in range(6)]

    return ([first, second] + [((4 - k) * first + (1 + k) * second) // 5 for k in range(4)]
            + [0, 255])


def dds_mean(blob, at):
    total = 0.0

    for value in struct.unpack_from("<2H", blob, at):
        total += ((value >> 11 & 31) * 255.0 / 31.0 + (value >> 5 & 63) * 255.0 / 63.0
                  + (value & 31) * 255.0 / 31.0) / 3.0

    return total / 2.0

#revise the uni2 fbx after
def dds_transparent(blob):
    if len(blob) < 148 or blob[:4] != b"DDS ":
        return False

    height, width = struct.unpack_from("<2I", blob, 12)
    fourcc = blob[84:88]
    body = blob[128:]
    count = (max(width, 4) // 4) * (max(height, 4) // 4)

    if fourcc == b"DXT1":
        for i in range(min(count, 4096)):
            at = i * 8

            if at + 8 > len(body):
                break

            low, high = struct.unpack_from("<2H", body, at)

            if low > high:
                continue

            bits = struct.unpack_from("<I", body, at + 4)[0]

            for t in range(16):
                if (bits >> (t * 2)) & 3 == 3:
                    return True

        return False

    if fourcc not in (b"DXT3", b"DXT5"):
        return False

    for i in range(min(count, 4096)):
        at = i * 16

        if at + 16 > len(body):
            break

        if fourcc == b"DXT5":
            if body[at] < 250 and body[at + 1] < 250:
                return True

            continue

        if min(body[at:at + 8]) != 0xff:
            return True

    return False


def dds_uniform(blob):
    body, step, colour, _alpha = dds_blocks(blob)

    if body is None:
        return False

    count = len(body) // step

    if count == 0:
        return False

    seen = set()
    stride = max(1, count // 4096)

    for i in range(0, count, stride):
        seen.add(body[i * step + colour:i * step + colour + 4])

        if len(seen) > 1:
            return False

    return bool(seen)


def dds_alphamix(blob):
    body, step, _colour, alpha = dds_blocks(blob)

    if body is None or alpha < 0:
        return (0.0, 0.0, 1.0)

    count = len(body) // step

    if count == 0:
        return (0.0, 0.0, 1.0)

    stride = max(1, count // 2048)
    inside = 0
    solid = 0
    total = 0

    for i in range(0, count, stride):
        at = i * step + alpha
        levels = dds_levels(body[at], body[at + 1])
        bits = int.from_bytes(body[at + 2:at + 8], "little")

        for k in range(16):
            value = levels[(bits >> (k * 3)) & 7]
            total += 1

            if value > SOFT_HIGH:
                solid += 1
            elif value >= SOFT_LOW:
                inside += 1

    if not total:
        return (0.0, 0.0, 1.0)

    return (1.0 - (inside + solid) / float(total), inside / float(total),
            solid / float(total))


def dds_greyscale(blob):
    body, step, colour, _alpha = dds_blocks(blob)

    if body is None:
        return 255.0

    count = len(body) // step

    if count == 0:
        return 255.0

    stride = max(1, count // 2048)
    spread = 0.0
    total = 0

    for i in range(0, count, stride):
        for value in struct.unpack_from("<2H", body, i * step + colour):
            red = (value >> 11 & 31) * 255.0 / 31.0
            green = (value >> 5 & 63) * 255.0 / 63.0
            blue = (value & 31) * 255.0 / 31.0
            spread += max(red, green, blue) - min(red, green, blue)
            total += 1

    return spread / total if total else 255.0


def dds_backing(blob):
    body, step, colour, alpha = dds_blocks(blob)

    if body is None or alpha < 0:
        return (0.0, 0.0)

    count = len(body) // step

    if count == 0:
        return (0.0, 0.0)

    stride = max(1, count // 2048)
    clear = 0
    total = 0
    level = 0.0

    for i in range(0, count, stride):
        at = i * step
        levels = dds_levels(body[at + alpha], body[at + alpha + 1])
        bits = int.from_bytes(body[at + alpha + 2:at + alpha + 8], "little")
        mean = dds_mean(body, at + colour)

        for k in range(16):
            total += 1

            if levels[(bits >> (k * 3)) & 7] < BACK_CLEAR:
                clear += 1
                level += mean

    if not total or not clear:
        return (0.0, 0.0)

    return (clear / float(total), level / clear)


def dds_lit(blob):
    share, level = dds_backing(blob)

    return share <= BACK_SHARE or level < BACK_LOW or level >= BACK_HIGH


def dds_sampler(blob):
    if len(blob) < 148 or blob[:4] != b"DDS ":
        return None

    height, width = struct.unpack_from("<2I", blob, 12)
    body, step, colour, _alpha = dds_blocks(blob)

    if body is None:
        return None

    wide, high = max(width // 4, 1), max(height // 4, 1)

    def at(u, v):
        x = min(max(int(u * wide), 0), wide - 1)
        y = min(max(int(v * high), 0), high - 1)
        p = (y * wide + x) * step + colour

        if p + 4 > len(body):
            return 0.0

        total = 0.0

        for value in struct.unpack_from("<2H", body, p):
            total += (value >> 11 & 31) * 8 + (value >> 5 & 63) * 4 + (value & 31) * 8

        return total / 6.0

    return at


def dds_haloed(blob, box):
    at = dds_sampler(blob)

    if at is None:
        return False

    ulo, uhi = max(box[0], 0.0), min(box[1], 1.0)
    vlo, vhi = max(box[2], 0.0), min(box[3], 1.0)

    if uhi - ulo < 0.01 or vhi - vlo < 0.01:
        return False

    steps = 16
    ring = []
    core = []

    for i in range(steps + 1):
        for j in range(steps + 1):
            u = ulo + (uhi - ulo) * i / steps
            v = vlo + (vhi - vlo) * j / steps
            edge = i <= 1 or j <= 1 or i >= steps - 1 or j >= steps - 1
            (ring if edge else core).append(at(u, v))

    dark = sum(ring) / len(ring)
    bright = sum(core) / len(core)

    return dark < 6.0 and bright > 4.0 * max(dark, 0.5) and bright > 20.0

#same decode to a small dds. (8x8 shit)
def dds_pixels(blob):
    if len(blob) < 148 or blob[:4] != b"DDS ":
        return None

    height, width = struct.unpack_from("<2I", blob, 12)

    if width * height > 64 * 64 or not width or not height:
        return None

    body, step, colour, alpha = dds_blocks(blob)

    if body is None:
        return None

    out = [0.0] * (width * height * 4)
    wide = max(width // 4, 1)

    for block in range(len(body) // step):
        at = block * step
        first, second = struct.unpack_from("<2H", body, at + colour)
        bits = struct.unpack_from("<I", body, at + colour + 4)[0]
        ends = []

        for value in (first, second):
            ends.append([(value >> 11 & 31) / 31.0, (value >> 5 & 63) / 63.0,
                         (value & 31) / 31.0])

        if first > second or alpha >= 0:
            ends.append([(2 * ends[0][k] + ends[1][k]) / 3.0 for k in range(3)])
            ends.append([(ends[0][k] + 2 * ends[1][k]) / 3.0 for k in range(3)])
        else:
            ends.append([(ends[0][k] + ends[1][k]) / 2.0 for k in range(3)])
            ends.append([0.0, 0.0, 0.0])

        shades = None

        if alpha >= 0:
            levels = dds_levels(body[at + alpha], body[at + alpha + 1])
            shades = int.from_bytes(body[at + alpha + 2:at + alpha + 8], "little")

        for k in range(16):
            x = (block % wide) * 4 + k % 4
            y = (block // wide) * 4 + k // 4

            if x >= width or y >= height:
                continue

            which = (bits >> (k * 2)) & 3
            clear = first <= second and which == 3 and alpha < 0
            opaque = 1.0

            if shades is not None:
                opaque = levels[(shades >> (k * 3)) & 7] / 255.0

            where = ((height - 1 - y) * width + x) * 4

            for c in range(3):
                out[where + c] = 0.0 if clear else ends[which][c]

            out[where + 3] = 0.0 if clear else opaque

    return width, height, out


def loaded_image(where):
    image = bpy.data.images.load(where, check_existing=True)

    if image.size[0]:
        return image

    with open(where, "rb") as handle:
        decoded = dds_pixels(handle.read())

    if decoded is None:
        return image

    width, height, pixels = decoded
    leaf = os.path.basename(where)
    bpy.data.images.remove(image)
    fresh = bpy.data.images.new(leaf, width, height, alpha=True)
    fresh.pixels = pixels
    fresh.pack()

    return fresh


def stage_material(model, slot, index, blend, soft, prefix):
    leaf = sheet_of(model, slot)
    unique_name = (material_name(model, slot, prefix) + ("" if blend == "opaque" else " " + blend))
    known = bpy.data.materials.get(unique_name)

    if known is not None:
        return known

    material = bpy.data.materials.new(unique_name)
    material.use_nodes = True
    tree = material.node_tree

    for node in list(tree.nodes):
        if node.type != "OUTPUT_MATERIAL":
            tree.nodes.remove(node)

    output = tree.nodes["Material Output"]
    texture = tree.nodes.new("ShaderNodeTexImage")
    texture.extension = "REPEAT"

    where = found_texture(index, leaf)

    if where:
        try:
            texture.image = loaded_image(where)
        except RuntimeError:
            pass

    coord = tree.nodes.new("ShaderNodeTexCoord")
    mapping = tree.nodes.new("ShaderNodeMapping")
    mapping.name = "Mapping"
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

    fade = tree.nodes.new("ShaderNodeMath")
    fade.operation = "MULTIPLY"
    tree.links.new(fade.inputs[0], texture.outputs["Alpha"])
    tree.links.new(fade.inputs[1], shade.outputs["Alpha"])

    if blend == "opaque":
        tree.nodes.remove(fade)
        tree.links.new(output.inputs[0], emission.outputs["Emission"])
    else:
        clear = tree.nodes.new("ShaderNodeBsdfTransparent")

        if blend == "add":
            mixer = tree.nodes.new("ShaderNodeAddShader")
            tree.links.new(mixer.inputs[0], emission.outputs["Emission"])
            tree.links.new(mixer.inputs[1], clear.outputs["BSDF"])
        else:
            mixer = tree.nodes.new("ShaderNodeMixShader")
            tree.links.new(mixer.inputs[0], fade.outputs["Value"])
            tree.links.new(mixer.inputs[1], clear.outputs["BSDF"])
            tree.links.new(mixer.inputs[2], emission.outputs["Emission"])

        tree.links.new(output.inputs[0], mixer.outputs[0])

        if (soft or blend == "add") and hasattr(material, "surface_render_method"):
            material.surface_render_method = "BLENDED"

    material["mua_slot"] = slot
    material["mua_texture"] = leaf
    material["mua_blendmode"] = 1 if blend == "add" else 0
    material["mua_material"] = model.value[slot] if 0 <= slot < len(model.value) else []

    return material


def scroll_material(material, keys, flip):
    if len(keys) < 2:
        return 0

    tree = material.node_tree
    mapping = tree.nodes.get("Mapping")

    if mapping is None:
        return 0

    frames = [key["frame"] for key in keys]
    action = fresh_action(tree, "%s flow" % material.name)
    written = 0

    channels = ((1, 0, lambda k: k["offset"][0]),
                (1, 1, lambda k: 1.0 - k["scale"][1] - k["offset"][1] if flip
                 else k["offset"][1]),
                (3, 0, lambda k: k["scale"][0]),
                (3, 1, lambda k: k["scale"][1]))

    for socket, index, reading in channels:
        values = [reading(key) for key in keys]

        if len(set(values)) < 2:
            continue

        write_curve(action, tree, 'nodes["Mapping"].inputs[%d].default_value' % socket, index,
                    frames, values)
        written += 1

    if not written:
        bpy.data.actions.remove(action)
        tree.animation_data_clear()

        return 0

    cycled(action)

    return written


class Paint(object):
    #Which meshes needs draw additive
    def __init__(self, model, index, ramped=()):
        self.ramped = set(ramped)
        self.cutout = []
        self.flat = []
        self.sheer = []
        self.grey = []
        self.glowing = []
        self.art = {}
        at, count = model.section[VERTEX_SECTION]
        self.opacity = any(model.blob[at + i * VERTEX + 0x37] == 255 for i in range(count))

        for name in model.texture:
            where = found_texture(index, os.path.basename(name).lower())
            art = b""

            if where:
                with open(where, "rb") as handle:
                    art = handle.read()

            _clear, fine, solid = dds_alphamix(art)
            self.cutout.append(dds_transparent(art))
            self.flat.append(dds_uniform(art))
            self.sheer.append(fine >= SOFT_SHARE or (solid < GLOW_SOLID and fine > GLOW_SOFT))
            self.grey.append(fine < SOFT_SHARE and dds_greyscale(art) < GREY_SPREAD)
            self.glowing.append(dds_lit(art))
            self.art[len(self.cutout) - 1] = art

    def sheet(self, model, slot):
        assigned = model.material[slot] if 0 <= slot < len(model.material) else []

        return assigned[0] if assigned and 0 <= assigned[0] < len(model.texture) else 0

    def blend(self, model, slots, kept):
        #Gambiarra de opaco precisa revisar esssa poha depois
        faded = self.opacity and any(one["colour"][3] < 1.0 for one in kept)
        cut = any(self.cutout[self.sheet(model, slot)] for slot in slots)
        soft = any(self.sheer[self.sheet(model, slot)] for slot in slots)

        if not faded and not cut:
            return "opaque", False

        return "alpha", soft or faded

    def lit_overlay(self, model, mesh, slots, kept):
        #0x12 wall lamps (central station) temp! [it should work for now tho]
        if model.scriptOf(mesh) in self.ramped:
            return True

        if any(one["colour"][3] < 1.0 for one in kept):
            return False

        if any(self.cutout[self.sheet(model, slot)] for slot in slots):
            return False

        black = sum(1 for one in kept if max(one["colour"][:3]) == 0.0)

        return (black >= 0.25 * len(kept)
                and max(max(one["colour"][:3]) for one in kept) >= 128.0 / 255.0)

    def additive(self, model, mesh, slots, kept, verts, triangles):
        if not kept or not triangles:
            return False

        if self.lit_overlay(model, mesh, slots, kept):
            return True

        faded = any(one["colour"][3] < 1.0 for one in kept)
        unlit = any(max(one["colour"][:3]) == 0.0 for one in kept)
        mean = [sum(one["colour"][c] for one in kept) / len(kept) for c in range(3)]
        masked = max(mean) < MASK_LEVEL
        box = (min(one["uv"][0] for one in kept), max(one["uv"][0] for one in kept),
               min(1.0 - one["uv"][1] for one in kept), max(1.0 - one["uv"][1] for one in kept))
        adds = False

        for slot in slots:
            index = self.sheet(model, slot)
            alight = self.glowing[index] and (self.sheer[index]
                                              or (self.cutout[index] and faded)
                                              or (self.grey[index] and masked))
            adds = (adds or alight or (self.flat[index] and unlit)
                    or dds_haloed(self.art.get(index, b""), box))

        if not adds:
            return False

        span = max(max(v[k] for v in verts) - min(v[k] for v in verts) for k in range(3))

        return len(triangles) <= ADDS_MOST and span <= ADDS_SPAN

# :) ..... :( 
def build_object(index, model, mesh, options, sheets, collection, paint, prefix):
    name = "node%03d %s" % (index, mesh["name"])
    data = bpy.data.meshes.new(name)
    obj = bpy.data.objects.new(name, data)
    collection.objects.link(obj)

    scale = options["scale"]
    verts = []
    kept = []
    weights = []

    for v in range(mesh["vertices"]):
        raw = model.vertex(mesh["firstVertex"] + v)
        x, y, z = raw["position"]
        nx, ny, nz = raw["normal"]

        verts.append((x * scale, y * scale, (-z if options["mirror"] else z) * scale))
        kept.append({
            "normal": (nx, ny, -nz if options["mirror"] else nz),
            "uv": (raw["uv"][0], 1.0 - raw["uv"][1] if options["flip"] else raw["uv"][1]),
            "colour": tuple(c / 255.0 for c in raw["colour"]),
        })
        weights.append(raw["weights"] or [(0, 1.0)])

    faces = []
    slots = []
    order = []
    seen = {}

    for p in range(mesh["parts"]):
        at = mesh["firstPart"] + p

        if at >= len(model.part):
            continue

        part = model.part[at]
        material = part["material"]

        if material not in seen:
            seen[material] = len(order)
            order.append(material)

        for a, b, c in model.triangles(part):
            if max(a, b, c) >= mesh["vertices"]:
                continue

            faces.append((a, c, b) if options["mirror"] else (a, b, c))
            slots.append(seen[material])

    if not verts or not faces:
        bpy.data.objects.remove(obj)

        return None, None

    additive = paint is not None and paint.additive(model, mesh, order, kept, verts, faces)
    blend, soft = paint.blend(model, order, kept) if paint is not None else ("opaque", False)

    if additive:
        blend, soft = "add", True

    for material in order:
        obj.data.materials.append(stage_material(model, material, sheets, blend, soft, prefix))

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
        uvs.data[loop.index].uv = kept[loop.vertex_index]["uv"]

    for i, source in enumerate(kept):
        shade.data[i].color = source["colour"]
        normals.data[i].vector = source["normal"]
        rest.data[i].vector = verts[i]

    first = obj.data.materials[0] if obj.data.materials else None
    obj["mua_mesh"] = mesh["name"]
    obj["mua_sheet"] = first.get("mua_texture", "") if first is not None else ""
    obj["mua_node"] = index
    obj["mua_skeleton"] = mesh["skeleton"]
    obj["mua_script"] = model.scriptOf(mesh)
    obj["fbxex_blendmode"] = 1 if additive else 0
    obj["fbxex_flag1"] = 0

    return obj, weights


def build_armature(model, options, name, collection):
    data = bpy.data.armatures.new(name)
    rig = bpy.data.objects.new(name, data)
    collection.objects.link(rig)

    view = bpy.context.view_layer
    was = view.objects.active
    view.objects.active = rig
    bpy.ops.object.mode_set(mode="EDIT")

    place = options["place"]
    world = [place @ as_matrix(model.world(i)) @ place.inverted()
             for i in range(len(model.bone))]
    least = max(options["scale"] * 40.0, 1e-4)
    children = {}

    for index, bone in enumerate(model.bone):
        parent = global_parent(model, index)

        if parent is not None:
            children.setdefault(parent, []).append(index)

    names = []
    made = []

    for index, bone in enumerate(model.bone):
        edit = data.edit_bones.new(unique(data.edit_bones, bone["name"] or "bone%03d" % index))
        head = world[index].translation
        own = children.get(index, [])
        towards = None

        if len(own) == 1:
            towards = world[own[0]].translation

        length = (towards - head).length if towards is not None else 0.0
        edit.head = head
        edit.tail = towards if length > least else (head[0], head[1] + least, head[2])
        names.append(edit.name)
        made.append(edit)

    for index in range(len(model.bone)):
        parent = global_parent(model, index)

        if parent is not None:
            made[index].parent = made[parent]

    bpy.ops.object.mode_set(mode="OBJECT")
    view.objects.active = was

    for bone in rig.pose.bones:
        bone.rotation_mode = "QUATERNION"

    return rig, names


def global_parent(model, index):
    if index not in model.owner:
        return None

    _which, first, _local = model.owner[index]
    parent = model.bone[index]["parent"]

    if not 0 <= parent < model.skeleton[_which][1] or first + parent == index:
        return None

    return first + parent


def bind(obj, weights, model, mesh, rig, names):
    first = mesh["bone"]

    if first < 0 or not weights:
        return False

    buckets = {}

    for index, listed in enumerate(weights):
        for local, weight in listed:
            at = first + local

            if not 0 <= at < len(names):
                continue

            buckets.setdefault((names[at], round(weight, 6)), []).append(index)

    if not buckets:
        return False

    groups = {}

    for (name, weight), indices in buckets.items():
        group = groups.get(name)

        if group is None:
            group = obj.vertex_groups.new(name=name)
            groups[name] = group

        group.add(indices, weight, "REPLACE")

    obj.parent = rig
    modifier = obj.modifiers.new(name="Armature", type="ARMATURE")
    modifier.object = rig
    modifier.use_vertex_groups = True

    return True


def deltas(model, first, count, take, place):
    #inverse * frame, dumb convention + blender unit
    rest = [model.bone[first + i]["matrix"] for i in range(count)]
    settled = [as_matrix(model.chain(first, i, rest)).inverted() for i in range(count)]
    unplace = place.inverted()

    moving = set(i for i in range(count) if take.moves(i))
    grown = True

    while grown:
        grown = False

        for i in range(count):
            parent = model.bone[first + i]["parent"]

            if i not in moving and 0 <= parent < count and parent in moving:
                moving.add(i)
                grown = True

    if not moving:
        return [], {}

    needed = set()

    for i in moving:
        local = i
        seen = set()

        while 0 <= local < count and local not in seen:
            seen.add(local)
            needed.add(local)
            local = model.bone[first + local]["parent"]

    frames = set()

    for i in needed:
        frames.update(take.keyframes(i))

    frames = sorted(f for f in frames if f >= 0)

    if len(frames) < 2:
        return [], {}

    out = dict((i, []) for i in moving)

    for frame in frames:
        local = dict((i, compose(*take.sample(i, frame, model.bone[first + i]["rest"])))
                     for i in needed)
        built = {}

        for i in needed:
            chained = model.chain(first, i, [local.get(k, rest[k]) for k in range(count)])
            built[i] = place @ (as_matrix(chained) @ settled[i]) @ unplace

        for i in moving:
            out[i].append(built[i])

    return frames, out


def build_action(rig, names, model, first, count, take, label, place):
    frames, moved = deltas(model, first, count, take, place)

    if not moved:
        return None

    action = fresh_action(rig, label)
    written = 0

    for local, steps in sorted(moved.items()):
        index = first + local

        if not 0 <= index < len(names):
            continue

        parent = model.bone[index]["parent"]
        rest = rig.data.bones[names[index]].matrix_local
        settled = rest.inverted()
        basis = []

        for at in range(len(frames)):
            delta = steps[at]

            if 0 <= parent < count and parent in moved:
                delta = moved[parent][at].inverted() @ delta

            basis.append(settled @ delta @ rest)

        written += keyed(action, rig, names[index], frames, basis)

    if not written:
        bpy.data.actions.remove(action)
        rig.animation_data.action = None

        return None

    return action


def keyed(action, rig, bone, frames, basis):
    path = 'pose.bones["%s"].' % bone
    broken = [step.decompose() for step in basis]
    rotations = [row[1] for row in broken]

    for at in range(1, len(rotations)):
        if rotations[at].dot(rotations[at - 1]) < 0.0:
            rotations[at] = -rotations[at]

    channels = (("location", [row[0] for row in broken], (0.0, 0.0, 0.0)),
                ("rotation_quaternion", rotations, (1.0, 0.0, 0.0, 0.0)),
                ("scale", [row[2] for row in broken], (1.0, 1.0, 1.0)))
    written = 0

    for name, rows, still in channels:
        for index in range(len(still)):
            values = [row[index] for row in rows]
            spread = max(values) - min(values)

            if spread <= 1e-7 and abs(values[0] - still[index]) <= 1e-7:
                continue

            if spread <= 1e-7:
                write_curve(action, rig, path + name, index, frames[:1], values[:1])
                written += 1
                continue

            write_curve(action, rig, path + name, index, frames, values)
            written += 1

    return written


def arrange(rig, actions, model, found, picked):
    #script own thing
    longest = 0

    for first in sorted(actions):
        label = model.bone[first]["name"] or "bone%03d" % first
        loop, entries = (0, [])

        if first in picked:
            loop, entries = found[picked[first]].picks()

        if not entries:
            for name in sorted(actions[first]):
                track = rig.animation_data.nla_tracks.new()
                track.name = "%s %s" % (label, name.rsplit(".", 1)[0])
                longest = max(longest, place_strip(track, actions[first][name], 0, 0))

            continue

        track = rig.animation_data.nla_tracks.new()
        track.name = picked[first]

        for at, (frame, leaf) in enumerate(entries):
            action = actions[first].get(leaf)
            ends = entries[at + 1][0] if at + 1 < len(entries) else loop

            if action is None:
                continue

            longest = max(longest, place_strip(track, action, frame, ends - frame))

        longest = max(longest, loop)

    return longest


def place_strip(track, action, start, span):
    strip = track.strips.new(action.name, int(start), action)

    if hasattr(strip, "action_slot") and len(action.slots):
        strip.action_slot = action.slots[0]

    strip.blend_type = "REPLACE"
    strip.use_auto_blend = False
    strip.extrapolation = "HOLD"

    if span > 0 and strip.action_frame_end - strip.action_frame_start > span:
        strip.action_frame_end = strip.action_frame_start + span
        strip.frame_end = start + span

    return strip.frame_end


def build_ramps(objects, script):
    loop, ramps = script.ramps()

    if not loop or not ramps:
        return 0

    frames = []
    values = []
    current = 0.0

    for at, target, over in ramps:
        goal = target / 1000.0
        frames.extend((at, min(loop, at + over)))
        values.extend((current, goal))
        current = goal

    for obj in objects:
        obj["mua_ramp"] = 0.0
        action = action_for(obj, "%s ramp" % obj.name)
        write_curve(action, obj, '["mua_ramp"]', 0, frames, values)
        cycled(action)

    return len(objects)


def framed(obj, rect, size, flip):
    x, y, w, h = rect
    wide, tall = size
    left = x / float(wide)
    right = (x + w) / float(wide)
    low = 1.0 - (y + h) / float(tall) if flip else y / float(tall)
    high = 1.0 - y / float(tall) if flip else (y + h) / float(tall)

    uvs = obj.data.uv_layers.active

    if uvs is None:
        return

    us = [uvs.data[loop.index].uv[0] for loop in obj.data.loops]
    vs = [uvs.data[loop.index].uv[1] for loop in obj.data.loops]
    lowU, lowV = min(us), min(vs)
    spanU = max(us) - lowU
    spanV = max(vs) - lowV

    for loop in obj.data.loops:
        uv = uvs.data[loop.index].uv
        acrossU = 0.0 if spanU <= 0.0 else (uv[0] - lowU) / spanU
        acrossV = 0.0 if spanV <= 0.0 else (uv[1] - lowV) / spanV
        uv[0] = left + (right - left) * acrossU
        uv[1] = low + (high - low) * acrossV


def sheet_named(leaf, sheet, named):
    #don't ask
    if named:
        return named[sheet] if 0 <= sheet < len(named) else leaf

    if sheet <= 0:
        return leaf

    stem, kind = os.path.splitext(leaf)
    digits = len(stem) - len(stem.rstrip("0123456789"))

    if not digits:
        return leaf

    return "%s%0*d%s" % (stem[:len(stem) - digits], digits, sheet, kind)


def build_rects(objects, script, sizes, collection, flip, index):
    loop, timeline = script.rects()

    if not loop or not any(timeline):
        return 0

    order = []

    for frame in timeline:
        if frame is not None and frame not in order:
            order.append(frame)

    built = 0

    for obj in objects:
        leaf = (obj.get("mua_sheet") or "").lower()

        if len(obj.data.vertices) != 4:
            continue

        for at, frame in enumerate(order):
            sheet, rect = frame[0], frame[1:]
            wanted = sheet_named(leaf, sheet, script.named).lower()

            if wanted not in sizes:
                wanted = leaf

            size = sizes.get(wanted)

            if size is None:
                continue

            shown = obj

            if at:
                shown = obj.copy()
                shown.data = obj.data.copy()
                shown.name = "%s f%d" % (obj.name, at)
                shown.animation_data_clear()
                collection.objects.link(shown)

                if obj.parent is not None:
                    shown.parent = obj.parent

            if wanted != leaf:
                swap_sheet(shown, wanted, index)

            framed(shown, rect, size, flip)
            keyed_visibility(shown, timeline, frame, loop)
            built += 1

    return built


def swap_sheet(obj, leaf, index):
    #same material, dif sheet
    if not obj.data.materials or obj.data.materials[0] is None:
        return

    material = obj.data.materials[0]
    name = "%s %s" % (material.name.split(" +")[0], os.path.splitext(leaf)[0])
    known = bpy.data.materials.get(name)

    if known is None:
        known = material.copy()
        known.name = name
        node = next((one for one in known.node_tree.nodes if one.type == "TEX_IMAGE"), None)
        where = index.get(leaf)

        if node is not None and where:
            try:
                node.image = loaded_image(where)
            except RuntimeError:
                pass

        known["mua_texture"] = leaf

    obj.data.materials[0] = known


def keyed_visibility(obj, timeline, rect, loop):
    frames = []
    values = []
    last = None

    for frame, shown in enumerate(timeline):
        hidden = 0.0 if shown == rect else 1.0

        if hidden != last:
            frames.append(frame)
            values.append(hidden)
            last = hidden

    if len(frames) < 2:
        return

    action = action_for(obj, "%s shown" % obj.name)

    for path in ("hide_viewport", "hide_render"):
        write_curve(action, obj, path, 0, frames, values, kind="CONSTANT")

    cycled(action)


def build_texts(found, stage):
    for name in sorted(found):
        text = bpy.data.texts.new("%s %s.evb" % (stage, name))
        text.write(found[name].listing())

    return len(found)


def bound_objects(model, built, name):
    out = []

    for index, mesh in enumerate(model.mesh):
        if model.scriptOf(mesh) == name and index in built:
            out.append(built[index])

    return out


def build_model(context, entry, model, files, found, options, sheets, label, prefix):
    ramped = [name for name, script in found.items() if script.ramps()[1]]
    paint = Paint(model, sheets, ramped) if sheets else None
    collection = bpy.data.collections.new(label)
    context.scene.collection.children.link(collection)
    built = {}
    weights = {}

    for index, mesh in enumerate(model.mesh):
        obj, listed = build_object(index, model, mesh, options, sheets, collection, paint,
                                   prefix)

        if obj is None:
            continue

        obj["mua_model"] = entry
        built[index] = obj
        weights[index] = listed

    used = []

    for obj in built.values():
        for material in obj.data.materials:
            if material is not None and material not in used:
                used.append(material)

    asked = [material for material in used if material.get("mua_texture")]
    report = {"mesh": len(built), "of": len(model.mesh), "take": 0, "flow": 0, "rect": 0,
              "ramp": 0, "skin": 0, "frames": 0, "wanted": len(asked),
              "texture": len([material for material in asked if textured(material)])}

    if options["flow"]:
        for material in used:
            slot = material.get("mua_slot")

            if slot is None:
                continue

            if scroll_material(material, model.materialFlow[int(slot)], options["flip"]):
                report["flow"] += 1

    if options["rig"]:
        rig, names = build_armature(model, options, label + "_bones", collection)

        for index, obj in built.items():
            if bind(obj, weights[index], model, model.mesh[index], rig, names):
                report["skin"] += 1

        if options["animation"]:
            report.update(animate(rig, names, model, files, found, options))

        rig.hide_set(True)

    if not found:
        return report

    sizes = sheet_sizes(sheets)

    for name, script in sorted(found.items()):
        objects = bound_objects(model, built, name)

        if not objects:
            continue

        report["ramp"] += build_ramps(objects, script)
        report["rect"] += build_rects(objects, script, sizes, collection, options["flip"], sheets)

    return report


def do_import(context, path, options):
    stage = stage_of(path)
    models, files, origin = gathered(path)
    where = [os.path.dirname(path)]

    if options["from"]:
        where.append(bpy.path.abspath(options["from"]))

    folders = unpack_textures(where, stage) if options["textures"] else []
    sheets = texture_index(folders)
    context.scene["mua_source"] = origin
    context.scene["mua_character"] = UNI2_CHARACTER / options["scale"]
    context.scene["mua_mirror"] = 1 if options["mirror"] else 0
    context.scene["mua_flip"] = 1 if options["flip"] else 0

    found = scripts(files) if options["scripts"] else {}
    report = {"mesh": 0, "of": 0, "take": 0, "flow": 0, "rect": 0, "ramp": 0, "skin": 0,
              "frames": 0, "wanted": 0, "texture": 0, "model": len(models),
              "script": build_texts(found, stage) + build_bins(files, stage)}

    for entry, model in models:
        alone = len(models) == 1
        label = stage if alone else os.path.splitext(os.path.basename(entry))[0]
        one = build_model(context, entry, model, files, found, options, sheets, label,
                          "" if alone else label)

        for key, value in one.items():
            report[key] = max(report[key], value) if key == "frames" else report[key] + value

    return report


def textured(material):
    tree = material.node_tree

    if tree is None:
        return False

    return any(node.type == "TEX_IMAGE" and node.image is not None for node in tree.nodes)


def show_materials(context):
    if context.screen is None:
        return

    for area in context.screen.areas:
        if area.type != "VIEW_3D":
            continue

        for space in area.spaces:
            if space.type == "VIEW_3D" and space.shading.type in ("WIREFRAME", "SOLID"):
                space.shading.type = "MATERIAL"


def animate(rig, names, model, files, found, options):
    made = takes(model, files)
    actions = {}
    picked = {}

    for first, group in sorted(made.items()):
        which = model.owner[first][0]
        count = model.skeleton[which][1]

        for label, take in sorted(group.items()):
            action = build_action(rig, names, model, first, count, take,
                                 "%s %s" % (model.bone[first]["name"], label.rsplit(".", 1)[0]),
                                 options["place"])

            if action is not None:
                actions.setdefault(first, {})[label] = action

    for index, mesh in enumerate(model.mesh):
        name = model.scriptOf(mesh)

        if name and name in found and mesh["bone"] in actions:
            picked[mesh["bone"]] = name

    if not actions:
        return {"take": 0, "frames": 0}

    rig.animation_data.action = None
    longest = arrange(rig, actions, model, found, picked)

    return {"take": sum(len(group) for group in actions.values()), "frames": int(longest)}

def placement(scale, mirror):
    z = -scale if mirror else scale

    return mathutils.Matrix.Diagonal((scale, scale, z, 1.0))


#still needs some fix
class ImportMua(bpy.types.Operator, ImportHelper):
    bl_idname = "import_scene.mua"
    bl_label = "Import Mua model"
    bl_options = {"UNDO"}

    filename_ext = ".pac"
    filter_glob: StringProperty(default="*.pac;*.mua;*.MUA", options={"HIDDEN"})
    
    #uni shit
    character: FloatProperty(
        name="Character height",
        description="How tall a character is in the model's own units. Lower it to make the model bigger",
        default=CHARACTER, min=1.0, max=10000.0)

    mirror: BoolProperty(
        name="Mirror in Z",
        description="Mirror the model on Z so it faces the way UNI2 does",
        default=True)

    flip: BoolProperty(
        name="Flip V",
        description="Flip the textures so they're the right way up",
        default=True)

    textures: BoolProperty(
        name="Textures",
        description="Load the textures from the archives beside the model",
        default=True)

    textures_from: StringProperty(
        name="Textures from",
        description="A second folder to look in for the textures",
        subtype="DIR_PATH",
        default="")

    rig: BoolProperty(
        name="Skeleton",
        description="Build the skeleton and skin the meshes to it",
        default=True)

    animation: BoolProperty(
        name="Animation",
        description="Import the motions and the model's own tracks",
        default=True)

    flow: BoolProperty(
        name="UV scroll",
        description="Import the scrolling textures",
        default=True)

    scripts: BoolProperty(
        name="Scripts",
        description="Import the scripts: motion timing, light ramps and sprite sequences",
        default=True)

    def execute(self, context):
        options = {
            "scale": UNI2_CHARACTER / self.character, #unit shit again needs to be redone 
            "mirror": self.mirror,
            "flip": self.flip,
            "textures": self.textures,
            "rig": self.rig,
            "animation": self.animation,
            "flow": self.flow,
            "scripts": self.scripts,
            "from": self.textures_from,
        }
        options["place"] = placement(options["scale"], self.mirror)

        try:
            report = do_import(context, self.filepath, options)
        except (ValueError, OSError, struct.error) as problem:
            self.report({"ERROR"}, str(problem))

            return {"CANCELLED"}

        if report["frames"]:
            context.scene.render.fps = FPS
            context.scene.frame_start = 0
            context.scene.frame_end = max(context.scene.frame_end, report["frames"])

        if report["texture"]:
            show_materials(context)

        if report["texture"] < report["wanted"]:
            self.report({"WARNING"}, "Found %d of %d texture(s). Point Textures from at the folder "
                                     "that has them" % (report["texture"], report["wanted"]))

        self.report({"INFO"}, "%d of %d mesh(es), %d skinned, %d texture(s), %d take(s), "
                              "%d scroll(s), %d script(s), %d ramp(ed), %d sprite(s)"
                    % (report["mesh"], report["of"], report["skin"], report["texture"],
                       report["take"], report["flow"], report["script"], report["ramp"],
                       report["rect"]))

        return {"FINISHED"}


STRIDES = (0x20, 0x130, 0xc0, 0x20, 0x50, 0x20, 0x10, 0x1c, 0x10, 0x20, 0x20, 0x10, 0x20, 0x50,
           2, 0x10, 1)


def section_of(model, index):
    at, count = model.section[index]

    return model.blob[at:at + count * STRIDES[index]]


def as_strip(triangles):
    #triangl list
    out = []

    for a, b, c in triangles:
        if out:
            out.extend((out[-1], a))

            if len(out) % 2:
                out.append(a)

        out.extend((a, b, c))

    return out


def written_vertex(block, vertex, weights):
    out = bytearray(block) if block else bytearray(VERTEX)
    struct.pack_into("<3f", out, 0x00, *vertex["position"])
    struct.pack_into("<3f", out, 0x0c, *vertex["normal"])
    struct.pack_into("<2f", out, 0x24, *vertex["uv"])
    colour = vertex["colour"]
    struct.pack_into("<4B", out, 0x34, colour[2], colour[1], colour[0], colour[3])

    if weights is not None:
        bones = [-1.0, -1.0, -1.0]
        shares = [-1.0, -1.0, -1.0]

        for k, (bone, weight) in enumerate(weights[:3]):
            bones[k] = float(bone)
            shares[k] = weight

        struct.pack_into("<3f", out, 0x38, *bones)
        struct.pack_into("<3f", out, 0x44, *shares)

    return bytes(out)


def bounds_block(points):
    #+0x18 8x 8y 8z
    low = [min(p[k] for p in points) for k in range(3)]
    high = [max(p[k] for p in points) for k in range(3)]
    mid = [(low[k] + high[k]) / 2.0 for k in range(3)]
    radius = max(math.sqrt(sum((p[k] - mid[k]) ** 2 for k in range(3))) for p in points)
    order = ((3, 0, 0, 3, 3, 0, 0, 3), (3, 3, 0, 0, 3, 3, 0, 0), (3, 3, 3, 3, 0, 0, 0, 0))
    corners = []

    for k in range(3):
        corners.extend(high[k] if which else low[k] for which in order[k])

    return struct.pack("<28f", *(mid + [radius] + corners))


def written_mua(model, rebuilt):
    #replace sec 2 3 13 14. needs to do others sections (I don't think I wanna do that to be clear)
    vertices = bytearray()
    indices = bytearray()
    meshes = bytearray()
    parts = bytearray()
    template = section_of(model, VERTEX_SECTION)
    first_part = 0

    for index, mesh in enumerate(model.mesh):
        own = rebuilt.get(index)
        record = bytearray(model.blob[model.section[MESH][0] + index * 0xc0:][:0xc0])
        first_vertex = len(vertices) // VERTEX

        if own is None:
            at = mesh["firstVertex"] * VERTEX
            vertices.extend(template[at:at + mesh["vertices"] * VERTEX])
            made = [(mesh["firstPart"] + p, None) for p in range(mesh["parts"])
                    if mesh["firstPart"] + p < len(model.part)]
        else:
            for at, vertex in enumerate(own["vertices"]):
                block = b""

                if own["kept"]:
                    where = (mesh["firstVertex"] + at) * VERTEX
                    block = template[where:where + VERTEX]

                if vertex is None:
                    vertices.extend(block)
                    continue

                vertices.extend(written_vertex(block, vertex,
                                               None if own["kept"] else vertex["weights"]))

            if any(vertex is not None for vertex in own["vertices"]):
                points = []

                for at, vertex in enumerate(own["vertices"]):
                    if vertex is not None:
                        points.append(vertex["position"])
                        continue

                    points.append(model.vertex(mesh["firstVertex"] + at)["position"])

                if points:
                    record[0x18:0x18 + 112] = bounds_block(points)

            made = []

            for order, fresh in enumerate(own["parts"]):
                at = mesh["firstPart"] + order
                same = (own["kept"] and order < mesh["parts"] and at < len(model.part)
                        and model.part[at]["material"] == fresh["material"]
                        and model.triangles(model.part[at]) == fresh["triangles"])
                made.append((at, None) if same else (at, fresh))

        for at, fresh in made:
            entry = bytearray(0x20)
            material = fresh["material"] if fresh is not None else model.part[at]["material"]

            if fresh is None:
                where = model.section[PART][0] + at * 0x20
                entry = bytearray(model.blob[where:where + 0x20])
                strip = model.strip(model.part[at])
            else:
                strip = as_strip(fresh["triangles"])

            struct.pack_into("<3i", entry, 0, material, len(strip), len(indices) // 2)

            for value in strip:
                indices.extend(struct.pack("<H", value))

            parts.extend(entry)

        struct.pack_into("<4I", record, 8, len(made), first_part,
                         len(vertices) // VERTEX - first_vertex, first_vertex)
        meshes.extend(record)
        first_part += len(made)

    fresh = {MESH: bytes(meshes), PART: bytes(parts), VERTEX_SECTION: bytes(vertices),
             INDEX: bytes(indices)}
    counts = {MESH: len(meshes) // 0xc0, PART: len(parts) // 0x20,
              VERTEX_SECTION: len(vertices) // VERTEX, INDEX: len(indices) // 2}

    out = bytearray(model.blob[:0x20 + SECTIONS * 8])
    at = len(out)

    for index in range(SECTIONS):
        body = fresh.get(index, section_of(model, index))
        count = counts.get(index, model.section[index][1])
        struct.pack_into("<2I", out, 0x20 + index * 8, at, count)
        out.extend(body)
        at += len(body)

    return bytes(out)

#lit end and pack (16 bytes)
def rewrapped(original, plain):
    if original[:4] != PACKED:
        return plain

    packed = zlib.compress(plain, 9)
    return struct.pack("<4s4s2I", PACKED, plain[:4], len(plain), len(packed)) + packed


def rebuilt_archive(blob, prefix, swap):
    out = {}

    for name, at, size in pac_entries(blob):
        body = blob[at:at + size]
        inner = unpacked(body)

        if is_archive(inner):
            fresh = rebuilt_archive(inner, prefix + name.rsplit(".", 1)[0] + "/", swap)

            if fresh is not None:
                out[name] = rewrapped(body, fresh)

            continue

        if prefix + name in swap:
            out[name] = swap[prefix + name]

    return written_pac(blob, out) if out else None


def with_mesh_table(model, table):
    out = bytearray(model.blob[:0x20 + SECTIONS * 8])
    at = len(out)

    for index in range(SECTIONS):
        body = table if index == MESH else section_of(model, index)
        struct.pack_into("<2I", out, 0x20 + index * 8, at, model.section[index][1])
        out.extend(body)
        at += len(body)

    return bytes(out)


def twin_of(path):
    leaf = os.path.basename(path)

    if not leaf.lower().endswith("_vtx.pac"):
        return ""

    where = os.path.join(os.path.dirname(path), leaf[:-len("_vtx.pac")] + ".pac")

    return where if os.path.isfile(where) else ""


def written_twin(where, tables):
    with open(where, "rb") as handle:
        raw = handle.read()

    plain = unpacked(decrypted(os.path.basename(where), raw))

    if not is_archive(plain):
        return None

    swap = {}

    for name, body in pac_walk(plain).items():
        if not name.lower().endswith(".mua"):
            continue

        model = Model(body)
        table = tables.get(os.path.basename(name).lower())

        if table is None or len(table) != model.section[MESH][1] * 0xc0:
            continue

        swap[name] = with_mesh_table(model, table)

    if not swap:
        return None

    fresh = rebuilt_archive(plain, "", swap)

    return rewrapped(raw, fresh) if fresh is not None else None


def repacked(template, swap):
    with open(template, "rb") as handle:
        raw = handle.read()

    plain = unpacked(decrypted(os.path.basename(template), raw))

    if not is_archive(plain):
        raise ValueError("%s isn't an archive, so the model can't go back into it"
                         % os.path.basename(template))

    fresh = rebuilt_archive(plain, "", swap)

    if fresh is None:
        raise ValueError("%s has none of the models in this scene"
                         % os.path.basename(template))

    return rewrapped(raw, fresh)


def local_bone(obj, group, mesh):
    rig = obj.parent

    if rig is None or rig.type != "ARMATURE":
        return -1

    at = rig.data.bones.find(group)

    return at - mesh["bone"] if at >= 0 else -1

#to do: a lot of things :skull:
def exported(model, objects, place, mirror, flip):
    out = {}
    unplace = place.inverted()

    for obj in objects:
        index = obj.get("mua_node")

        if index is None or not 0 <= int(index) < len(model.mesh):
            continue

        index = int(index)
        mesh = model.mesh[index]
        data = obj.data
        kept = len(data.vertices) == mesh["vertices"]
        anchor = data.attributes.get("fbxex_rest")
        stored = data.attributes.get("fbxex_normal")
        shade = data.color_attributes.get("Shade")
        uvs = data.uv_layers.active
        corner = {}

        for loop in data.loops:
            corner.setdefault(loop.vertex_index,
                              tuple(uvs.data[loop.index].uv) if uvs is not None else (0.0, 0.0))

        groups = {}

        for group in obj.vertex_groups:
            groups[group.index] = local_bone(obj, group.name, mesh)

        rows = []

        for at, vertex in enumerate(data.vertices):
            placed = obj.matrix_world @ vertex.co
            still = (kept and anchor is not None and at < len(anchor.data)
                     and tuple(anchor.data[at].vector) == tuple(placed))

            if still:
                rows.append(None)
                continue

            point = unplace @ placed
            normal = (stored.data[at].vector if stored is not None and at < len(stored.data)
                      else vertex.normal)
            uv = corner.get(at, (0.0, 0.0))
            colour = (shade.data[at].color[:] if shade is not None and at < len(shade.data)
                      else (1.0, 1.0, 1.0, 1.0))
            weights = sorted(((groups.get(one.group, -1), one.weight) for one in vertex.groups),
                             key=lambda pair: -pair[1])
            rows.append({
                "position": (point.x, point.y, point.z),
                "normal": (normal[0], normal[1], -normal[2] if mirror else normal[2]),
                "uv": (uv[0], 1.0 - uv[1] if flip else uv[1]),
                "colour": [max(0, min(255, int(round(c * 255.0)))) for c in colour],
                "weights": [(bone, weight) for bone, weight in weights if bone >= 0],
            })

        parts = []

        for slot in range(max(1, len(data.materials))):
            material = data.materials[slot] if slot < len(data.materials) else None
            which = material.get("mua_slot") if material is not None else None
            triangles = []

            for polygon in data.polygons:
                if polygon.material_index != slot:
                    continue

                loop = list(polygon.vertices)

                for k in range(1, len(loop) - 1):
                    a, b, c = loop[0], loop[k], loop[k + 1]
                    triangles.append((a, c, b) if mirror else (a, b, c))

            if triangles:
                parts.append({"material": int(which) if which is not None else 0,
                              "triangles": triangles})

        out[index] = {"vertices": rows, "parts": parts, "kept": kept}

    return out


def upright(objects, turn):
    for obj in objects:
        obj.matrix_world = turn @ obj.matrix_world

#maybe theres a plugin texture for that, search that later (dds)
def as_png(folder):
    kept = []

    if not os.path.isdir(folder):
        os.makedirs(folder)

    for image in bpy.data.images:
        if not image.users or not image.filepath_raw:
            continue

        if image.filepath_raw.lower().endswith(".png"):
            continue

        if not image.has_data:
            try:
                image.pixels[0]
            except (RuntimeError, IndexError):
                continue

        if not image.has_data:
            continue

        leaf = os.path.splitext(os.path.basename(image.filepath_raw))[0] + ".png"
        where = os.path.join(folder, leaf)
        was = image.filepath_raw
        image.filepath_raw = where

        try:
            image.file_format = "PNG"
            image.save()
        except (RuntimeError, OSError):
            image.filepath_raw = was
            continue

        kept.append((image, was, where))

    return kept


def as_they_were(kept):
    for image, filepath, made in kept:
        image.filepath_raw = filepath

        if os.path.exists(made):
            os.remove(made)

#BSDF
def through_principled():
    kept = []

    for material in bpy.data.materials:
        tree = material.node_tree

        if tree is None:
            continue

        output = next((node for node in tree.nodes if node.type == "OUTPUT_MATERIAL"), None)
        texture = next((node for node in tree.nodes
                        if node.type == "TEX_IMAGE" and node.image is not None), None)

        if output is None or texture is None or not output.inputs[0].links:
            continue

        was = output.inputs[0].links[0].from_socket
        stood = tree.nodes.new("ShaderNodeBsdfPrincipled")
        stood.inputs["Roughness"].default_value = 1.0
        tree.links.new(stood.inputs["Base Color"], texture.outputs["Color"])
        tree.links.new(stood.inputs["Alpha"], texture.outputs["Alpha"])
        tree.links.new(output.inputs[0], stood.outputs[0])
        kept.append((tree, output, was, stood))

    return kept


def as_they_drew(kept):
    for tree, output, socket, stood in kept:
        tree.nodes.remove(stood)
        tree.links.new(output.inputs[0], socket)


class ExportModelFbx(bpy.types.Operator, ExportHelper):
    bl_idname = "export_scene.mua_fbx"
    bl_label = "Export FBX"
    bl_options = {"PRESET"}

    filename_ext = ".fbx"
    filter_glob: StringProperty(default="*.fbx", options={"HIDDEN"})

    selected: BoolProperty(
        name="Selected only",
        description="Write only the selected objects",
        default=False)

    animation: EnumProperty(
        name="Animation",
        items=(("NLA", "NLA tracks", "One take per NLA strip, the way the scripts lay them out"), #still needs to investigate more but mostly true
               ("ALL", "Every action", "Every action in the file, whether arranged or not"),
               ("NONE", "None", "Geometry only")),
        default="NLA")

    textures: EnumProperty(
        name="Textures",
        items=(("PNG", "PNG beside the FBX", "Converted to PNG in <name>.fbm, so any FBX program "
                                             "can open them"),
               ("EMBED", "Embedded as PNG", "Packed inside the FBX. One file, but a big one"),
               ("KEEP", "The DDS as they are", "Left as DDS where they are. Most programs can't "
                                               "open them")),
        default="PNG")

    def execute(self, context):
        turn = mathutils.Matrix.Rotation(math.pi / 2.0, 4, "X")
        roots = [obj for obj in context.scene.objects if obj.parent is None]
        converted = []
        upright(roots, turn)
        drawn = through_principled()

        if self.textures != "KEEP":
            converted = as_png(os.path.join(bpy.app.tempdir, "mua_png"))

        try:
            return bpy.ops.export_scene.fbx(
                filepath=self.filepath,
                use_selection=self.selected,
                object_types={"ARMATURE", "MESH", "EMPTY"},
                add_leaf_bones=False,
                path_mode="AUTO" if self.textures == "KEEP" else "COPY",
                embed_textures=self.textures == "EMBED",
                bake_anim=self.animation != "NONE",
                bake_anim_use_all_bones=True,
                bake_anim_use_nla_strips=self.animation == "NLA",
                bake_anim_use_all_actions=self.animation == "ALL",
                bake_anim_force_startend_keying=False)
        finally:
            as_they_were(converted)
            as_they_drew(drawn)
            upright(roots, turn.inverted())

#To do better way to keep everything opened
class ExportMua(bpy.types.Operator, ExportHelper):
    bl_idname = "export_scene.mua"
    bl_label = "Export Mua model"
    bl_description = "Write the model back, as the archive the game reads or as a bare .MUA"
    bl_options = {"PRESET"}

    filename_ext = ".pac"
    check_extension = None
    filter_glob: StringProperty(default="*.pac;*.mua;*.MUA", options={"HIDDEN"})

    template: StringProperty(
        name="Template",
        description="The model you opened. Blender can't hold the bones, the materials, the "
                    "animation or the scripts, so they come from this file",
        subtype="FILE_PATH",
        default="")

    selected: BoolProperty(
        name="Selected only",
        description="Write back only the selected objects",
        default=False)

    twin: BoolProperty(
        name="Update the scene archive",
        description="A stage keeps a second copy of its model in <stage>.pac. Moving things "
                    "changes its bounds, so that copy gets written too",
        default=True)

    def invoke(self, context, event):
        self.template = context.scene.get("mua_source", "")

        if self.template:
            self.filepath = self.template

        return ExportHelper.invoke(self, context, event)

    def execute(self, context):
        where = bpy.path.abspath(self.template or context.scene.get("mua_source", ""))

        if not where or not os.path.isfile(where):
            self.report({"ERROR"}, "Set Template to the model you opened")

            return {"CANCELLED"}

        character = float(context.scene.get("mua_character", CHARACTER))
        mirror = bool(context.scene.get("mua_mirror", 1))
        flip = bool(context.scene.get("mua_flip", 1))
        scale = UNI2_CHARACTER / character
        place = placement(scale, mirror)

        try:
            models, _files, origin = gathered(where)
        except (ValueError, OSError, struct.error) as problem:
            self.report({"ERROR"}, str(problem))

            return {"CANCELLED"}

        objects = [obj for obj in (context.selected_objects if self.selected
                                   else context.scene.objects)
                   if obj.type == "MESH" and obj.get("mua_node") is not None]
        swap = {}
        written = 0
        total = 0

        for entry, model in models:
            own = [obj for obj in objects
                   if obj.get("mua_model", entry if len(models) == 1 else None) == entry]
            rebuilt = exported(model, own, place, mirror, flip)
            swap[entry] = written_mua(model, rebuilt)
            written += len(rebuilt)
            total += len(model.mesh)

        archive = self.filepath.lower().endswith(".pac")

        if not archive and len(swap) > 1:
            self.report({"ERROR"}, "This file has %d models. Save it as a .pac" % len(swap))

            return {"CANCELLED"}

        body = swap[models[0][0]]
        kind = "model"

        if archive:
            try:
                body = repacked(origin, swap)
                kind = "archive"
            except ValueError as problem:
                self.report({"ERROR"}, str(problem))

                return {"CANCELLED"}

        try:
            with open(self.filepath, "wb") as handle:
                handle.write(body)
        except (OSError, struct.error) as problem:
            self.report({"ERROR"}, str(problem))

            return {"CANCELLED"}

        beside = ""

        if archive and self.twin:
            tables = {}

            for entry, fresh in swap.items():
                tables[os.path.basename(entry).lower()] = section_of(Model(fresh), MESH)

            where = twin_of(self.filepath)
            body = written_twin(where, tables) if where else None

            if body is not None:
                with open(where, "wb") as handle:
                    handle.write(body)

                beside = ", %s too" % os.path.basename(where)

        self.report({"INFO"}, "%s: %d model(s), %d of %d mesh(es) from the scene, %d kept%s"
                    % (kind, len(swap), written, total, total - written, beside))

        return {"FINISHED"}


def menu_import(self, context):
    self.layout.operator(ImportMua.bl_idname, text="Mua model (.mua, .pac)")


def menu_export(self, context):
    self.layout.operator(ExportMua.bl_idname, text="Mua model (.mua)")
    self.layout.operator(ExportModelFbx.bl_idname, text="Mua model as FBX (.fbx)")


def register():
    bpy.utils.register_class(ImportMua)
    bpy.utils.register_class(ExportMua)
    bpy.utils.register_class(ExportModelFbx)
    bpy.types.TOPBAR_MT_file_import.append(menu_import)
    bpy.types.TOPBAR_MT_file_export.append(menu_export)


def unregister():
    bpy.types.TOPBAR_MT_file_export.remove(menu_export)
    bpy.types.TOPBAR_MT_file_import.remove(menu_import)
    bpy.utils.unregister_class(ExportModelFbx)
    bpy.utils.unregister_class(ExportMua)
    bpy.utils.unregister_class(ImportMua)


if __name__ == "__main__":
    register()
