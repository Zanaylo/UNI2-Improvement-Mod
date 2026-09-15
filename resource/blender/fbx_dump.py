import json
import math
import os
import sys

import bpy
import mathutils


def dumped(path):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=os.path.abspath(path))

    turn = mathutils.Matrix.Rotation(-math.pi / 2.0, 4, "X")
    out = {}

    for obj in bpy.context.scene.objects:
        if obj.type != "MESH" or obj.data is None:
            continue

        place = turn @ obj.matrix_world
        out[obj.name] = [list(place @ vertex.co) for vertex in obj.data.vertices]

    return out


def main(argv):
    if len(argv) < 2:
        raise SystemExit("usage: fbx_dump.py -- <in.fbx> <out.json>")

    found = dumped(argv[0])

    with open(os.path.abspath(argv[1]), "w", encoding="utf-8") as handle:
        json.dump(found, handle)

    print("fbx_dump: %d mesh(es), %d vertex(es)"
          % (len(found), sum(len(v) for v in found.values())))


if __name__ == "__main__":
    main(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:])
