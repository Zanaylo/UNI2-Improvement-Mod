import argparse
import io
import os
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))

MANIFEST = '''schema_version = "1.0.0"

id = "%s"
version = "%s"
name = "%s"
tagline = "%s"
maintainer = "PrimoZanaylo"
type = "add-on"

blender_version_min = "4.2.0"

license = [
  "-",
]
'''

VERSION = '0.9.0'

ADDONS = (
    ('io_scene_fbxex', 'FbxExp model (.fbx.bin)', 'Import and export stage models'),
    ('io_scene_mua', 'Mua model (.mua)', 'Import a Mua model with its animation and scripts'),
)


def build(name, title, tagline):
    source = os.path.join(HERE, name + '.py')
    target = os.path.join(HERE, name + '.zip')
    body = io.open(source, encoding='utf-8').read()

    with zipfile.ZipFile(target, 'w', zipfile.ZIP_DEFLATED) as out:
        out.writestr('blender_manifest.toml', MANIFEST % (name, VERSION, title, tagline))
        out.writestr('__init__.py', body)

    print('%s  %d bytes' % (target, os.path.getsize(target)))


def main():
    argparse.ArgumentParser().parse_args()

    for row in ADDONS:
        build(*row)


if __name__ == '__main__':
    main()
