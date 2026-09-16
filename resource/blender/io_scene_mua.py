#OBS: Não sei se vou fazer manutenção dessa poha aqui. o importer para o UNI2 ainda está fudido mas agradeço por ler ou traduzir essa mensagem :)

"""Read AWS `MUA` model that is on bobleis games.
mot -> stage character or even effect -> skeleton, mashes 
(ca) mmot -> model own bones tracks, uv scroll
evb -> scripts
"""

bl_info = {
    "name": "Mua model (.mua)",
    "author": "PrimoZanaylo",
    "version": (0, 9, 0),
    "blender": (4, 2, 0),
    "location": "File > Import > Mua model",
    "description": "Import a Mua model with its animation and scripts",
    "category": "Import-Export",
}

import hashlib
import math
import os
import re
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
EVB_YIELD, EVB_TIME, EVB_CLOSE, EVB_GROUP = 0x02, 0x03, 0x04, 0x05
EVB_MOTION, EVB_BEGIN, EVB_END, EVB_RECT, EVB_LOOP, EVB_RAMP = 0x06, 0x09, 0x0a, 0x0b, 0x0f, 0x12
EVB_RANDOM, EVB_RANDOM_END, EVB_PAUSE = 0x13, 0x14, 0x15
EVB_TAKEN, EVB_SKIPPED, EVB_DONE = 2, 1, 3
EVB_FRAMES = 12000
EVB_ROLLED_FRAMES = 3600
ROLL_TRIES = 16
LCG_MULTIPLY, LCG_ADD, LCG_MASK = 1103515245, 12345, 0xffffffff
EVB_NONE = 0xffffffff
RAMP_FULL = 1000.0
RAMP_EPSILON = 1e-6

BLEND_OPAQUE, BLEND_ADD, BLEND_SUBTRACT, BLEND_UNSET = 0, 2, 4, 0x7fffffff
BASE_LAYER, SHINE_LAYER = 1, 3
HIDDEN_FLAG = 0x20
NO_DEPTH_WRITE_FLAG = 0x2000
SOFT_SHARE = 0.5
GLOW_SOLID = 0.01
GLOW_SOFT = 0.05
SOFT_SAMPLES = 4096
CARD_BLACK = 8.0 / 255.0
RIM_TAIL = 48.0 / 255.0
RIM_STEPS = 16
BLENDED_KINDS = ("add", "sub", "blend", "sheer")
SOFT_KINDS = ("blend", "sheer")

KEY = bytes((
    0xf5, 0x5c, 0x84, 0x2a, 0xad, 0x61, 0x54, 0xe7, 0x0a, 0xfc, 0x99, 0x6b, 0xd5, 0xa4, 0xd3, 0xd8,
    0x48, 0x26, 0x69, 0xcb, 0x07, 0x42, 0x13, 0x5e, 0x10, 0x23, 0xd2, 0x6d, 0x36, 0xc7, 0xc1, 0x66,
    0xdf, 0xa1, 0xad, 0xf1, 0x44, 0x44, 0x7e, 0xc9, 0x8e, 0x24, 0x99,
))

MAGIC = (b"FPAC", b"MUA\x00", b"MMOT", b"EVT0", b"DDS ", b"DFAS")
PACKED = b"DFAS"

IDENTITY = [1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0]

WINDOW_COVER = 0.9
WINDOW_BAND = 0.6
WINDOW_MATCH = 0.25
WINDOW_BANDS = 8
SHADE_GAMMA = 2.2
DDS_CAPS2 = 0x70
VIEW_TRANSFORM = "Standard"


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
        layer = []
        self.flow = []
        self.value = []

        for i in range(self._count(ASSIGN)):
            at = self._at(ASSIGN, i, 0x20)
            slot, texture = struct.unpack_from("<2i", self.blob, at)
            assign.append(texture)
            layer.append(slot)
            count, first = struct.unpack_from("<2i", self.blob, at + 8)
            self.flow.append([self._uvkey(first + k) for k in range(count)
                              if 0 <= first + k < self._count(UVANIM)])

        self.material = []
        self.materialLayers = []
        self.materialFlow = []

        for i in range(self._count(MATERIAL)):
            at = self._at(MATERIAL, i, 0x50)
            count, first = struct.unpack_from("<2i", self.blob, at)
            taken = sorted((first + k for k in range(count) if 0 <= first + k < len(assign)),
                           key=lambda k: layer[k] != BASE_LAYER)
            self.material.append([assign[k] for k in taken])
            self.materialLayers.append(dict((layer[k], assign[k]) for k in taken))
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
        self.skeletonBlend = []
        self.skeletonFlags = []

        for i in range(self._count(SKELETON)):
            at = self._at(SKELETON, i, 0x20)
            self.skeleton.append(struct.unpack_from("<2I", self.blob, at))
            self.skeletonScript.append(struct.unpack_from("<i", self.blob, at + 8)[0])
            self.skeletonBlend.append(dword(self.blob, at + 0xc))
            self.skeletonFlags.append(dword(self.blob, at + 0x10))

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
        return self.scriptAt(mesh["skeleton"])

    def scriptAt(self, which):
        if not 0 <= which < len(self.skeletonScript):
            return ""

        index = self.skeletonScript[which]

        if not 0 <= index < len(self.script):
            return ""

        named = self.script[index]

        return named.rsplit(".", 1)[0]

    def blendOf(self, mesh):
        which = mesh["skeleton"]

        if not 0 <= which < len(self.skeletonBlend):
            return BLEND_UNSET

        return self.skeletonBlend[which]

    def flagsOf(self, mesh):
        which = mesh["skeleton"]

        if not 0 <= which < len(self.skeletonFlags):
            return 0

        return self.skeletonFlags[which]
        
#Todo: optimize the load script processing + 


def evb_names(blob, at, count, stride):
    out = []

    for index in range(count):
        where = at + index * stride

        if where + stride > len(blob):
            break

        out.append(blob[where:where + stride].split(b"\x00")[0].decode("ascii", "replace"))

    return out


class Script(object):
    #EVT0 7 blocks 0x20, sheets in 0x20, names in 0x22, records off 0x10

    def __init__(self, blob):
        if blob[:4] != b"EVT0":
            raise ValueError("not an EVT0 script")

        _version, _names, self.last, commands, stride = struct.unpack_from("<5I", blob, 4)
        counts = struct.unpack_from("<7H", blob, 0x20)

        self.sheets = evb_names(blob, EVB_BLOCKS, counts[0], stride)
        self.named = evb_names(blob, EVB_BLOCKS + counts[0] * stride, counts[1], stride)
        record = [struct.unpack_from("<8I", blob, where)
                  for where in range(commands, len(blob) - EVB_RECORD + 1, EVB_RECORD)]
        ended = next((at for at, fields in enumerate(record) if fields[0] == EVB_NONE), len(record))
        self.record = record[:ended + 1]

    def run(self, label):
        instance = Instance(self, label)
        played = instance.played()
        played.rolled = instance.rolled

        return played

    def listing(self):
        out = ["sheets: %s" % (", ".join(self.sheets) or "-"),
               "names: %s" % (", ".join(self.named) or "-"), ""]

        for fields in self.record:
            if fields[0] == EVB_NONE:
                out.append("end")
                break

            args = " ".join(("%d" % f) if f != EVB_NONE else "-" for f in fields[1:])
            out.append("op 0x%02x  %s" % (fields[0], args))

        return "\n".join(out) + "\n"

def signed(value):
    return value - 0x100000000 if value >= 0x80000000 else value


class Dice(object):
    def __init__(self, label):
        self.state = zlib.crc32(label.encode("utf-8")) & LCG_MASK

    def percent(self):
        self.state = (self.state * LCG_MULTIPLY + LCG_ADD) & LCG_MASK

        return ((self.state >> 16) & 0x7fff) % 100


class Instance(object):
    def __init__(self, script, label):
        self.record = script.record
        self.named = script.named
        self.dice = Dice(label)
        self.labels = {}

        for at, fields in enumerate(self.record):
            if fields[0] == EVB_BEGIN:
                self.labels.setdefault(fields[1], at)

        self.frame = 0
        self.jump = -1
        self.pause = 0
        self.resume = 0
        self.label = -1
        self.cursor = -1
        self.held = 0
        self.rect = None
        self.ramp = RAMP_FULL
        self.target = RAMP_FULL
        self.left = 0
        self.step = 0.0
        self.picked = None
        self.rolled = False

    def played(self):
        self.interpret()
        self.walk()
        samples = []
        seen = {}

        for tick in range(EVB_FRAMES):
            if tick:
                self.tick()

            samples.append(self.sample())
            self.picked = None

            if self.rolled and not self.ended():
                if tick + 1 >= EVB_ROLLED_FRAMES:
                    return Run(samples, True, self.named)

                continue

            state = self.state()

            if state in seen:
                return repeating(samples, seen[state], tick, self.named)

            seen[state] = tick

        return Run(samples, False, self.named)

    def sample(self):
        if self.ramp <= 0.0:
            return (None, 0.0, self.picked)

        return (self.rect, self.ramp, self.picked)

    def ended(self):
        if self.jump >= 0:
            return False

        return (self.resume >= len(self.record)
                or self.record[self.resume][0] in (EVB_YIELD, EVB_NONE))

    def state(self):
        return (None if self.ended() else self.frame, self.jump, self.pause, self.resume,
                self.label, self.cursor, self.held, self.rect, self.ramp, self.target,
                self.left, self.step)

    def tick(self):
        if self.pause >= 1:
            self.pause -= 1
        elif self.jump < 0:
            self.frame += 1
        else:
            self.frame, self.jump, self.resume = self.jump, -1, 0

        if self.label >= 0:
            self.held += 1

        if self.left < 1:
            self.ramp = self.target
        else:
            self.ramp += self.step
            self.left -= 1

        self.ramp = min(max(self.ramp, 0.0), RAMP_FULL)

        if self.pause < 1:
            self.interpret()

        self.walk()

    def interpret(self):
        at = self.resume
        live = False
        opened = 0
        chosen = -1
        rolling = 0

        while at < len(self.record):
            fields = self.record[at]
            code = fields[0]

            if code in (EVB_YIELD, EVB_NONE):
                break

            if code == EVB_TIME and self.frame < fields[1]:
                break

            if code == EVB_TIME:
                if self.frame == fields[1]:
                    live, opened = True, fields[1]
            elif code == EVB_CLOSE:
                live = False
            elif code == EVB_RANDOM:
                rolling = self.roll(rolling, fields[1])
            elif code == EVB_RANDOM_END:
                rolling = 0
            elif live and rolling in (0, EVB_TAKEN):
                chosen = self.command(fields, opened, chosen)

            at += 1

        self.resume = at

        if chosen in self.labels:
            self.label = chosen
            self.cursor = self.labels[chosen]

    def roll(self, rolling, percent):
        if rolling in (EVB_TAKEN, EVB_DONE):
            return EVB_DONE

        self.rolled = True

        return EVB_TAKEN if self.dice.percent() < percent else EVB_SKIPPED

    def command(self, fields, opened, chosen):
        code = fields[0]

        if code == EVB_GROUP:
            self.held = self.frame - opened

            return fields[1]

        if code == EVB_MOTION:
            self.picked = signed(fields[1])
        elif code == EVB_LOOP:
            self.jump = signed(fields[1])
        elif code == EVB_RAMP:
            self.target = float(fields[1])
            self.left = signed(fields[2]) or 1
            self.step = (self.target - self.ramp) / self.left
        elif code == EVB_PAUSE:
            self.pause = signed(fields[1])

        return chosen

    def walk(self):
        if self.label < 0:
            return

        start = self.held
        at = self.cursor

        for _ in range(2 * len(self.record)):
            if not 0 <= at < len(self.record) or self.record[at][0] == EVB_NONE:
                return

            fields = self.record[at]

            if fields[0] == EVB_RECT and self.held < fields[1]:
                self.rect = tuple(fields[2:7])
                self.cursor = at

                return

            if fields[0] == EVB_RECT:
                self.held -= fields[1]
            elif fields[0] == EVB_END:
                span = start - self.held

                if span <= 0 or span == self.held:
                    return

                if span < self.held:
                    self.held %= span

                at = self.labels[self.label]

            at += 1


class Run(object):
    def __init__(self, samples, cyclic, named):
        self.length = len(samples)
        self.cyclic = cyclic
        self.rects = [one[0] for one in samples]
        self.ramps = [one[1] / RAMP_FULL for one in samples]
        self.picks = [(tick, named[one[2]] if 0 <= one[2] < len(named) else "")
                      for tick, one in enumerate(samples) if one[2] is not None]
        self.rolled = False

    def shows(self):
        return max(self.ramps[1:] or self.ramps) > 0.0

    def fades(self):
        return len(set(self.ramps)) > 1

    def holdsPartial(self):
        return any(RAMP_EPSILON < a < 1.0 - RAMP_EPSILON and abs(a - b) <= RAMP_EPSILON
                   for a, b in zip(self.ramps, self.ramps[1:]))


def repeating(samples, first, now, named):
    period = now - first
    start = first + 1

    while start > 0 and samples[start - 1] == samples[start - 1 + period]:
        start -= 1

    if start == 0:
        return Run(samples[:period], True, named)

    if period == 1:
        return Run(samples[:start + 1], False, named)

    if start <= period:
        steady = [samples[tick + period * -(-(start - tick) // period)] if tick < start
                  else samples[tick] for tick in range(period)]

        return Run(steady, True, named)

    while len(samples) < EVB_FRAMES:
        samples.append(samples[len(samples) - period])

    return Run(samples, False, named)


def script_runs(model, found):
    out = {}

    for skeleton in sorted(set(mesh["skeleton"] for mesh in model.mesh)):
        name = model.scriptAt(skeleton)

        if name in found:
            out[skeleton] = shown_run(found[name], "%s %d" % (name, skeleton))

    return out


def shown_run(script, label):
    run = None

    for attempt in range(ROLL_TRIES):
        run = script.run(label if not attempt else "%s %d" % (label, attempt))

        if run.shows() or not run.rolled:
            return run

    return run

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


def dds_feathered(blob):
    body, step, _colour, _alpha = dds_blocks(blob)

    if body is None or step != 16:
        return False

    explicit = blob[84:88] == b"DXT3"
    count = len(body) // step
    stride = max(1, count // SOFT_SAMPLES)
    soft = 0
    solid = 0
    total = 0

    for block in range(0, count, stride):
        for value in alpha_texels(body, block * step, explicit):
            total += 1
            soft += 0 < value < 255
            solid += value == 255

    if not total:
        return False

    fine = soft / float(total)

    return fine >= SOFT_SHARE or (solid / float(total) < GLOW_SOLID and fine > GLOW_SOFT)


def alpha_texels(body, at, explicit):
    if explicit:
        bits = int.from_bytes(body[at:at + 8], "little")

        return [((bits >> (k * 4)) & 15) * 17 for k in range(16)]

    levels = dds_levels(body[at], body[at + 1])
    bits = int.from_bytes(body[at + 2:at + 8], "little")

    return [levels[(bits >> (k * 3)) & 7] for k in range(16)]


def clear_texel(body, at, k):
    first, second, bits = struct.unpack_from("<2HI", body, at)

    return first <= second and (bits >> (k * 2)) & 3 == 3


def punched(body, at):
    return any(clear_texel(body, at, k) for k in range(16))


def block_palette(first, second, punchable):
    ends = [[(value >> 11 & 31) / 31.0, (value >> 5 & 63) / 63.0, (value & 31) / 31.0]
            for value in (first, second)]

    if first > second or not punchable:
        return ends + [[(2 * ends[0][k] + ends[1][k]) / 3.0 for k in range(3)],
                       [(ends[0][k] + 2 * ends[1][k]) / 3.0 for k in range(3)]]

    return ends + [[(ends[0][k] + ends[1][k]) / 2.0 for k in range(3)], [0.0, 0.0, 0.0]]


def texel_peak(body, at, punchable):
    first, second, bits = struct.unpack_from("<2HI", body, at)
    palette = block_palette(first, second, punchable)

    return max(max(palette[(bits >> (k * 2)) & 3]) for k in range(16))


def dds_glowing(blob):
    body, step, colour, _alpha = dds_blocks(blob)

    if body is None:
        return False

    explicit = blob[84:88] == b"DXT3"
    count = len(body) // step
    stride = max(1, count // SOFT_SAMPLES)
    peak = 0.0

    for block in range(0, count, stride):
        at = block * step

        if step == 8 and punched(body, at):
            return False

        if step == 16 and min(alpha_texels(body, at, explicit)) < 255:
            return False

        peak = max(peak, texel_peak(body, at + colour, step == 8))

    return peak > CARD_BLACK


def dds_sampler(blob):
    body, step, colour, _alpha = dds_blocks(blob)

    if body is None:
        return None

    height, width = struct.unpack_from("<2I", blob, 12)
    wide, high = max((width + 3) // 4, 1), max((height + 3) // 4, 1)

    def peak(u, v):
        at = ((math.floor(v * high) % high) * wide + math.floor(u * wide) % wide) * step + colour

        return texel_peak(body, at, step == 8) if at + 8 <= len(body) else 1.0

    return peak


class AlphaSheet(object):
    def __init__(self, blob):
        self.body, self.step, _colour, _alpha = dds_blocks(blob)
        self.explicit = blob[84:88] == b"DXT3"
        self.height, self.width = struct.unpack_from("<2I", blob, 12) if self.body else (0, 0)
        self.wide = max((self.width + 3) // 4, 1)

    def texel(self, x, y):
        x, y = x % self.width, y % self.height
        at = ((y // 4) * self.wide + x // 4) * self.step
        k = (y % 4) * 4 + x % 4

        if at + self.step > len(self.body):
            return 1.0

        if self.step == 8:
            return 0.0 if clear_texel(self.body, at, k) else 1.0

        return alpha_texels(self.body, at, self.explicit)[k] / 255.0

    def sample(self, u, v):
        x, y = u * self.width - 0.5, v * self.height - 0.5
        left, top = math.floor(x), math.floor(y)
        across, down = x - left, y - top
        upper = self.texel(left, top) * (1.0 - across) + self.texel(left + 1, top) * across
        lower = self.texel(left, top + 1) * (1.0 - across) + self.texel(left + 1, top + 1) * across

        return upper * (1.0 - down) + lower * down

    def faded(self):
        if not self.body:
            return False

        ring = ([(x, y) for x in range(self.width) for y in (0, self.height - 1)]
                + [(x, y) for y in range(self.height) for x in (0, self.width - 1)])

        return all(self.texel(x, y) == 0.0 for x, y in ring)

    def peak(self):
        if not self.body:
            return 0.0

        if self.step == 8:
            return 1.0

        count = len(self.body) // self.step
        stride = max(1, count // SOFT_SAMPLES)

        return max(max(alpha_texels(self.body, block * self.step, self.explicit))
                   for block in range(0, count, stride)) / 255.0

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
        ends = block_palette(first, second, alpha < 0)
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
    known = next((one for one in bpy.data.images if one.get("mua_source") == where), None)

    if known is not None:
        return known

    image = bpy.data.images.load(where, check_existing=True)

    if image.size[0]:
        return image

    with open(where, "rb") as handle:
        blob = handle.read()

    fresh = decoded_image(where, blob) or flattened_image(where, blob)

    if fresh is None:
        return image

    bpy.data.images.remove(image)
    fresh.name = os.path.basename(where)
    fresh["mua_source"] = where

    return fresh


def decoded_image(where, blob):
    decoded = dds_pixels(blob)

    if decoded is None:
        return None

    width, height, pixels = decoded
    fresh = bpy.data.images.new(os.path.basename(where), width, height, alpha=True)
    fresh.pixels = pixels
    fresh.pack()

    return fresh


def flattened_image(where, blob):
    if len(blob) < 128 or blob[:4] != b"DDS " or not dword(blob, DDS_CAPS2):
        return None

    copy = os.path.join(bpy.app.tempdir, os.path.basename(where))

    with open(copy, "wb") as handle:
        handle.write(blob[:DDS_CAPS2] + bytes(4) + blob[DDS_CAPS2 + 4:])

    fresh = bpy.data.images.load(copy)

    if not fresh.size[0]:
        bpy.data.images.remove(fresh)

        return None

    fresh.pack()

    return fresh


def shined(tree, model, slot, index, colour):
    layers = model.materialLayers[slot] if 0 <= slot < len(model.materialLayers) else {}
    texture = layers.get(SHINE_LAYER, -1)

    if not 0 <= texture < len(model.texture):
        return colour

    where = found_texture(index, os.path.basename(model.texture[texture]).lower())

    if not where:
        return colour

    geometry = tree.nodes.new("ShaderNodeNewGeometry")
    view = tree.nodes.new("ShaderNodeVectorTransform")
    view.vector_type = "NORMAL"
    view.convert_from = "WORLD"
    view.convert_to = "CAMERA"
    tree.links.new(view.inputs["Vector"], geometry.outputs["Normal"])

    sphere = tree.nodes.new("ShaderNodeMapping")
    sphere.name = "Shine"
    sphere.inputs["Location"].default_value = (0.5, 0.5, 0.0)
    sphere.inputs["Scale"].default_value = (0.5, 0.5, 1.0)
    tree.links.new(sphere.inputs["Vector"], view.outputs["Vector"])

    image = tree.nodes.new("ShaderNodeTexImage")
    image.extension = "EXTEND"

    try:
        image.image = loaded_image(where)
    except RuntimeError:
        return colour

    tree.links.new(image.inputs["Vector"], sphere.outputs["Vector"])

    glow = tree.nodes.new("ShaderNodeMix")
    glow.data_type = "RGBA"
    glow.blend_type = "ADD"
    glow.inputs["Factor"].default_value = 1.0
    tree.links.new(glow.inputs[6], colour)
    tree.links.new(glow.inputs[7], image.outputs["Color"])

    return glow.outputs[2]


def trimmed(tree, alpha):
    rim = tree.nodes.new("ShaderNodeAttribute")
    rim.attribute_type = "OBJECT"
    rim.attribute_name = "mua_rim"

    lifted = tree.nodes.new("ShaderNodeMath")
    lifted.operation = "SUBTRACT"
    tree.links.new(lifted.inputs[0], alpha)
    tree.links.new(lifted.inputs[1], rim.outputs["Fac"])

    span = tree.nodes.new("ShaderNodeMath")
    span.operation = "SUBTRACT"
    span.inputs[0].default_value = 1.0
    tree.links.new(span.inputs[1], rim.outputs["Fac"])

    scaled = tree.nodes.new("ShaderNodeMath")
    scaled.operation = "DIVIDE"
    scaled.use_clamp = True
    tree.links.new(scaled.inputs[0], lifted.outputs["Value"])
    tree.links.new(scaled.inputs[1], span.outputs["Value"])

    return scaled.outputs["Value"]


def stage_material(model, slot, index, blend, prefix):
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

    linear = tree.nodes.new("ShaderNodeGamma")
    linear.inputs["Gamma"].default_value = SHADE_GAMMA
    tree.links.new(linear.inputs["Color"], shade.outputs["Color"])

    tint = tree.nodes.new("ShaderNodeMix")
    tint.data_type = "RGBA"
    tint.blend_type = "MULTIPLY"
    tint.inputs["Factor"].default_value = 1.0
    tree.links.new(tint.inputs[6], texture.outputs["Color"])
    tree.links.new(tint.inputs[7], linear.outputs["Color"])

    emission = tree.nodes.new("ShaderNodeEmission")
    tree.links.new(emission.inputs["Color"], shined(tree, model, slot, index, tint.outputs[2]))

    fade = tree.nodes.new("ShaderNodeMath")
    fade.operation = "MULTIPLY"
    tree.links.new(fade.inputs[0], trimmed(tree, texture.outputs["Alpha"]) if blend in SOFT_KINDS
                   else texture.outputs["Alpha"])
    tree.links.new(fade.inputs[1], shade.outputs["Alpha"])

    ramp = tree.nodes.new("ShaderNodeAttribute")
    ramp.attribute_type = "OBJECT"
    ramp.attribute_name = "mua_ramp"
    ramped = tree.nodes.new("ShaderNodeMath")
    ramped.operation = "MULTIPLY"
    tree.links.new(ramped.inputs[0], fade.outputs["Value"])
    tree.links.new(ramped.inputs[1], ramp.outputs["Fac"])
    fade = ramped

    if blend == "opaque":
        cutout = tree.nodes.new("ShaderNodeMath")
        cutout.operation = "GREATER_THAN"
        cutout.inputs[1].default_value = 0.0
        tree.links.new(cutout.inputs[0], fade.outputs["Value"])
        fade = cutout

    clear = tree.nodes.new("ShaderNodeBsdfTransparent")

    if blend == "sub":
        tree.nodes.remove(emission)
        shadow = tree.nodes.new("ShaderNodeInvert")
        tree.links.new(shadow.inputs["Color"], tint.outputs[2])
        dim = tree.nodes.new("ShaderNodeMix")
        dim.data_type = "RGBA"
        dim.inputs[6].default_value = (1.0, 1.0, 1.0, 1.0)
        tree.links.new(dim.inputs["Factor"], fade.outputs["Value"])
        tree.links.new(dim.inputs[7], shadow.outputs["Color"])
        tree.links.new(clear.inputs["Color"], dim.outputs[2])
        mixer = clear
    elif blend == "add":
        tree.links.new(emission.inputs["Strength"], fade.outputs["Value"])
        mixer = tree.nodes.new("ShaderNodeAddShader")
        tree.links.new(mixer.inputs[0], emission.outputs["Emission"])
        tree.links.new(mixer.inputs[1], clear.outputs["BSDF"])
    else:
        mixer = tree.nodes.new("ShaderNodeMixShader")
        tree.links.new(mixer.inputs[0], fade.outputs["Value"])
        tree.links.new(mixer.inputs[1], clear.outputs["BSDF"])
        tree.links.new(mixer.inputs[2], emission.outputs["Emission"])

    tree.links.new(output.inputs[0], mixer.outputs[0])

    if hasattr(material, "surface_render_method"):
        material.surface_render_method = "BLENDED" if blend in BLENDED_KINDS else "DITHERED"

    material["mua_slot"] = slot
    material["mua_texture"] = leaf
    material["mua_blendmode"] = 1 if blend == "add" else 0
    material["mua_material"] = model.value[slot] if 0 <= slot < len(model.value) else []

    return material


def divisor(value):
    return value if value else 1.0


def uv_channels(flip):
    return ((1, 0, lambda k: -k["offset"][0] / divisor(k["scale"][0])),
            (1, 1, lambda k: 1.0 - (1.0 + k["offset"][1]) / divisor(k["scale"][1]) if flip
             else k["offset"][1] / divisor(k["scale"][1])),
            (3, 0, lambda k: 1.0 / divisor(k["scale"][0])),
            (3, 1, lambda k: 1.0 / divisor(k["scale"][1])))


def scroll_material(material, keys, flip):
    tree = material.node_tree
    mapping = tree.nodes.get("Mapping") if tree is not None else None

    if not keys or mapping is None:
        return 0

    channels = uv_channels(flip)

    for socket, index, reading in channels:
        mapping.inputs[socket].default_value[index] = reading(keys[0])

    if len(keys) < 2:
        return 0

    frames = [key["frame"] for key in keys]
    action = fresh_action(tree, "%s flow" % material.name)
    written = 0

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


# :) ..... :( 
def drawn_as(model, mesh, sheer, dark):
    mode = model.blendOf(mesh)

    if mode == BLEND_OPAQUE:
        return "opaque"

    if mode == BLEND_ADD:
        return "add"

    if mode == BLEND_SUBTRACT:
        return "sub"

    if model.flagsOf(mesh) & NO_DEPTH_WRITE_FLAG:
        return "sheer"

    if sheer:
        return "blend"

    return "add" if dark() else "alpha"


def see_through(model, slots, kept, run, art):
    if run is not None and run.holdsPartial():
        return True

    alphas = [one["colour"][3] for one in kept]

    if min(alphas) < 1.0 and max(alphas) > 0.0:
        return True

    return any(art.feathered(sheet_of(model, slot)) for slot in slots)


def black_card(model, slots, kept, art):
    if not slots:
        return False

    for slot in slots:
        leaf = sheet_of(model, slot)

        if not art.glows(leaf):
            return False

        peak = art.sampler(leaf)

        if any(peak(*one["texel"]) * max(one["colour"][:3]) > CARD_BLACK for one in kept):
            return False

    return True


def border(verts, faces, slots):
    touched = {}

    for face, slot in zip(faces, slots):
        for a, b in ((face[0], face[1]), (face[1], face[2]), (face[2], face[0])):
            touched.setdefault(frozenset((verts[a], verts[b])), []).append((a, b, slot))

    return [found[0] for found in touched.values() if len(found) == 1]


def rim_of(model, order, kept, edges, art):
    leaves = [sheet_of(model, material) for material in order]

    if not edges or not all(art.faded(leaf) for leaf in leaves):
        return 0.0

    rim = 0.0

    for a, b, slot in edges:
        sheet = art.alpha(leaves[slot])
        (ua, va), (ub, vb) = kept[a]["texel"], kept[b]["texel"]

        for step in range(RIM_STEPS + 1):
            share = step / float(RIM_STEPS)
            rim = max(rim, sheet.sample(ua + (ub - ua) * share, va + (vb - va) * share))

    return rim if rim <= RIM_TAIL * max(art.peak(leaf) for leaf in leaves) else 0.0


class SheetArt(object):
    def __init__(self, index):
        self.index = index
        self.soft = {}
        self.lit = {}
        self.samplers = {}
        self.alphas = {}
        self.clear = {}
        self.peaks = {}

    def read(self, leaf):
        where = found_texture(self.index, leaf)

        if not where:
            return b""

        with open(where, "rb") as handle:
            return handle.read()

    def feathered(self, leaf):
        if leaf not in self.soft:
            self.soft[leaf] = dds_feathered(self.read(leaf))

        return self.soft[leaf]

    def glows(self, leaf):
        if leaf not in self.lit:
            self.lit[leaf] = dds_glowing(self.read(leaf))

        return self.lit[leaf]

    def sampler(self, leaf):
        if leaf not in self.samplers:
            self.samplers[leaf] = dds_sampler(self.read(leaf))

        return self.samplers[leaf]

    def alpha(self, leaf):
        if leaf not in self.alphas:
            self.alphas[leaf] = AlphaSheet(self.read(leaf))

        return self.alphas[leaf]

    def faded(self, leaf):
        if leaf not in self.clear:
            self.clear[leaf] = self.alpha(leaf).faded()

        return self.clear[leaf]

    def peak(self, leaf):
        if leaf not in self.peaks:
            self.peaks[leaf] = self.alpha(leaf).peak()

        return self.peaks[leaf]


def build_object(index, model, mesh, options, sheets, collection, prefix, runs, art):
    if model.flagsOf(mesh) & HIDDEN_FLAG:
        return None, None

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
            "texel": tuple(raw["uv"]),
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

    sheer = see_through(model, order, kept, runs.get(mesh["skeleton"]), art)
    blend = drawn_as(model, mesh, sheer, lambda: black_card(model, order, kept, art))
    rim = rim_of(model, order, kept, border(verts, faces, slots), art) if blend in SOFT_KINDS else 0.0

    for material in order:
        obj.data.materials.append(stage_material(model, material, sheets, blend, prefix))

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
    obj["mua_ramp"] = 1.0
    obj["mua_rim"] = rim
    obj["fbxex_blendmode"] = 1 if blend == "add" else 0
    obj["mua_blend"] = model.blendOf(mesh)
    obj["mua_flags"] = model.flagsOf(mesh)
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


def arrange(rig, actions, model, runs, picked):
    #script own thing
    longest = 0

    for first in sorted(actions):
        label = model.bone[first]["name"] or "bone%03d" % first
        run = runs.get(picked.get(first))
        entries = run.picks if run is not None else []

        if not entries:
            for name in sorted(actions[first]):
                track = rig.animation_data.nla_tracks.new()
                track.name = "%s %s" % (label, name.rsplit(".", 1)[0])
                longest = max(longest, place_strip(track, actions[first][name], 0, 0))

            continue

        loop = run.length if run.cyclic else 0
        track = rig.animation_data.nla_tracks.new()
        track.name = model.scriptAt(picked[first])

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


def changes(values, run):
    frames = [0]
    kept = [values[0]]

    for tick in range(1, len(values)):
        if values[tick] != values[tick - 1]:
            frames.append(tick)
            kept.append(values[tick])

    return closed(frames, kept, run)


def bends(values, run):
    frames = [0]
    kept = [values[0]]

    for tick in range(1, len(values) - 1):
        if abs((values[tick] - values[tick - 1]) - (values[tick + 1] - values[tick])) > RAMP_EPSILON:
            frames.append(tick)
            kept.append(values[tick])

    if len(values) > 1:
        frames.append(len(values) - 1)
        kept.append(values[-1])

    return closed(frames, kept, run)


def closed(frames, values, run):
    if run.cyclic:
        frames.append(run.length)
        values.append(values[0])

    return frames, values


def cycled_curve(fcurve, run):
    if run.cyclic and not any(one.type == "CYCLES" for one in fcurve.modifiers):
        fcurve.modifiers.new("CYCLES")


def keyed_sequence(obj, run, shown):
    hidden = [0.0 if on and ramp > 0.0 else 1.0 for on, ramp in zip(shown, run.ramps)]
    obj["mua_ramp"] = run.ramps[0]

    if len(set(hidden)) > 1:
        frames, values = changes(hidden, run)
        action = action_for(obj, "%s script" % obj.name)

        for path in ("hide_viewport", "hide_render"):
            cycled_curve(write_curve(action, obj, path, 0, frames, values, kind="CONSTANT"), run)
    elif hidden[0]:
        obj.hide_viewport = True
        obj.hide_render = True

    if not run.fades():
        return

    frames, values = bends(run.ramps, run)
    action = action_for(obj, "%s script" % obj.name)
    cycled_curve(write_curve(action, obj, '["mua_ramp"]', 0, frames, values), run)


def framed(obj, rect, size, flip):
    x, y, w, h = rect
    wide, tall = size
    uvs = obj.data.uv_layers.active

    if uvs is None:
        return

    for corner in uvs.data:
        u, v = corner.uv
        down = 1.0 - v if flip else v
        across = (y + down * h) / float(tall)
        corner.uv = ((x + u * w) / float(wide), 1.0 - across if flip else across)


def distinct(timeline):
    order = []

    for frame in timeline:
        if frame is not None and frame not in order:
            order.append(frame)

    return order


def copied(obj, at, collection):
    shown = obj.copy()
    shown.data = obj.data.copy()
    shown.name = "%s f%d" % (obj.name, at)
    shown.animation_data_clear()
    collection.objects.link(shown)

    if obj.parent is not None:
        shown.parent = obj.parent

    return shown


def placements(order, leaf, script, sizes):
    out = []

    for frame in order:
        sheet = frame[0]
        wanted = script.sheets[sheet].lower() if 0 <= sheet < len(script.sheets) else leaf

        if wanted in sizes:
            out.append((frame, wanted))

    return out


def sprites(obj, run, script, sizes, collection, flip, index):
    leaf = (obj.get("mua_sheet") or "").lower()
    placed = placements(distinct(run.rects), leaf, script, sizes)
    fitted = set(rect for rect, _wanted in placed)

    if not placed:
        return [(obj, None)], fitted

    bare = any(rect not in fitted for rect in run.rects)
    count = len(placed) + (1 if bare else 0)
    targets = [obj] + [copied(obj, at, collection) for at in range(1, count)]
    out = [(targets.pop(0), None)] if bare else []

    for target, (rect, wanted) in zip(targets, placed):
        if wanted != leaf:
            swap_sheet(target, wanted, index)

        framed(target, rect[1:], sizes[wanted], flip)
        out.append((target, rect))

    return out, fitted


def shown_ticks(run, rect, fitted):
    if rect is None:
        return [one not in fitted for one in run.rects]

    return [one == rect for one in run.rects]


def build_sequence(objects, run, script, sizes, collection, flip, index):
    placed = 0

    for obj in objects:
        targets, fitted = sprites(obj, run, script, sizes, collection, flip, index)

        for target, rect in targets:
            keyed_sequence(target, run, shown_ticks(run, rect, fitted))
            placed += rect is not None

    return placed, (len(objects) if run.fades() else 0)


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


def build_texts(found, stage):
    for name in sorted(found):
        text = bpy.data.texts.new("%s %s.evb" % (stage, name))
        text.write(found[name].listing())

    return len(found)


def build_model(context, entry, model, files, found, options, sheets, label, prefix):
    runs = script_runs(model, found)
    art = SheetArt(sheets)
    collection = bpy.data.collections.new(label)
    context.scene.collection.children.link(collection)
    built = {}
    weights = {}

    for index, mesh in enumerate(model.mesh):
        obj, listed = build_object(index, model, mesh, options, sheets, collection, prefix,
                                   runs, art)

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
            report.update(animate(rig, names, model, files, runs, options))

        rig.hide_set(True)

    if not runs:
        return report

    sizes = sheet_sizes(sheets)

    for skeleton, run in sorted(runs.items()):
        objects = [built[index] for index, mesh in enumerate(model.mesh)
                   if mesh["skeleton"] == skeleton and index in built]

        if not objects:
            continue

        placed, ramped = build_sequence(objects, run, found[model.scriptAt(skeleton)], sizes,
                                        collection, options["flip"], sheets)
        report["rect"] += placed
        report["ramp"] += ramped

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
    context.scene.view_settings.view_transform = VIEW_TRANSFORM

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


def animate(rig, names, model, files, runs, options):
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
        if mesh["skeleton"] in runs and mesh["bone"] in actions:
            picked[mesh["bone"]] = mesh["skeleton"]

    if not actions:
        return {"take": 0, "frames": 0}

    rig.animation_data.action = None
    longest = arrange(rig, actions, model, runs, picked)

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
