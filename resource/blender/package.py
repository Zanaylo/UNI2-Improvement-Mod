"""Package the add-on as a Blender extension zip.

Blender 4.2 introduced extensions and **Install from Disk expects a zip with a manifest**, so a bare
`.py` either does nothing or lands as a legacy add-on that still has to be ticked by hand. This
builds the zip that installs and enables in one step.

    python resource/blender/package.py

It writes `resource/blender/io_scene_fbxex.zip` from the same `io_scene_fbxex.py` the tests run
against, so the packaged add-on is never a separate copy that can drift.
"""

import argparse
import io
import os
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
SOURCE = os.path.join(HERE, 'io_scene_fbxex.py')
TARGET = os.path.join(HERE, 'io_scene_fbxex.zip')

ID = 'io_scene_fbxex'
VERSION = '1.1.0'

MANIFEST = '''schema_version = "1.0.0"

id = "%s"
version = "%s"
name = "FbxExp stage (.fbx.bin)"
tagline = "Import and export UNI2, MELTY BLOOD and UNI stage models"
maintainer = "UNI2 Improvement Mod"
type = "add-on"

blender_version_min = "4.2.0"

license = [
  "SPDX:GPL-3.0-or-later",
]
''' % (ID, VERSION)


def main():
    argparse.ArgumentParser().parse_args()

    body = io.open(SOURCE, encoding='utf-8').read()

    with zipfile.ZipFile(TARGET, 'w', zipfile.ZIP_DEFLATED) as out:
        out.writestr('blender_manifest.toml', MANIFEST)
        out.writestr('__init__.py', body)

    print(TARGET)
    print('%d bytes' % os.path.getsize(TARGET))


if __name__ == '__main__':
    main()
