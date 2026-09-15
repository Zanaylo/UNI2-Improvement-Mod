import argparse
import os
import sys

import bpy

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import io_scene_mua as mua


def parsed(argv):
    parser = argparse.ArgumentParser(prog='mua_to_fbx')
    parser.add_argument('model')
    parser.add_argument('fbx', nargs='?')
    parser.add_argument('--character', type=float, default=mua.CHARACTER,
                        help='how tall a character is in the model units, 213 fits BBTAG')
    parser.add_argument('--keep-facing', action='store_true',
                        help="keep BBTAG's facing instead of mirroring it for UNI2")
    parser.add_argument('--keep-v', action='store_true', help='leave the textures as the DDS has them')
    parser.add_argument('--static', action='store_true', help='geometry only')
    parser.add_argument('--no-textures', action='store_true')
    parser.add_argument('--animation', choices=('NLA', 'ALL', 'NONE'), default='NLA')
    parser.add_argument('--textures', choices=('PNG', 'EMBED', 'KEEP'), default='PNG')
    parser.add_argument('--textures-from', default='',
                        help='another folder to look for textures in')

    return parser.parse_args(argv)


def options(settings):
    scale = mua.UNI2_CHARACTER / settings.character
    mirror = not settings.keep_facing
    live = not settings.static

    return {
        'scale': scale,
        'mirror': mirror,
        'flip': not settings.keep_v,
        'textures': not settings.no_textures,
        'from': settings.textures_from,
        'rig': True,
        'animation': live,
        'flow': live,
        'scripts': live,
        'place': mua.placement(scale, mirror),
    }


def convert(settings):
    where = os.path.abspath(settings.model)
    out = settings.fbx or os.path.join(os.path.dirname(where), mua.stage_of(where) + '.fbx')

    bpy.ops.wm.read_factory_settings(use_empty=True)
    report = mua.do_import(bpy.context, where, options(settings))

    if report['frames']:
        bpy.context.scene.render.fps = mua.FPS
        bpy.context.scene.frame_start = 0
        bpy.context.scene.frame_end = report['frames']

    animation = 'NONE' if settings.static else settings.animation
    mua.register()
    bpy.ops.export_scene.mua_fbx(filepath=os.path.abspath(out), animation=animation,
                                   textures=settings.textures)

    print('%s -> %s  %s' % (os.path.basename(where), os.path.abspath(out),
                            ', '.join('%s=%s' % (k, report[k]) for k in sorted(report))))

    return out


def main(argv):
    settings = parsed(argv)

    if not os.path.isfile(settings.model):
        raise SystemExit('%s is not a file' % settings.model)

    convert(settings)


if __name__ == '__main__':
    main(sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else sys.argv[1:])
