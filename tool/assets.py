import glob
import os
import sys
import errno
import shutil
import json

from textureatlas import TextureAtlas, Texture, Frame
from PIL import Image


ATLAS_WIDTH = 8192
ATLAS_HEIGHT = 8192

def mkdir_p(path):
    try:
        os.makedirs(path)
    except OSError as exc:
        if exc.errno == errno.EEXIST and os.path.isdir(path):
            pass
        else:
            raise


def main():
    outdir = sys.argv[1]

    TEXTURES = ["atlas.png"]
    SPRITES = dict()

    atlas = TextureAtlas(ATLAS_WIDTH, ATLAS_HEIGHT)

    atlas_tex = []
    for dirname in ["PathOfBuilding/TreeData", "PathOfBuilding/TreeData/legion", "PathOfBuilding/TreeData/3_19", "PathOfBuilding/Assets"]:
        mkdir_p(os.path.join(outdir, dirname))
        for filetype in ["*.jpg", "*.png"]:
            for filepath in glob.glob(os.path.join(dirname, filetype)):
                if "/Background" not in filepath and "LineConnector" not in filepath and "ring.png" not in filepath:
                    print("[atlas] {}".format(filepath))
                    atlas_tex.append(Texture(filepath, [Frame(filepath)]))
                else:
                    print("[raw] {}".format(filepath))

                    # dispX, dispY, scaleX, scaleY
                    # tgtX = dispX + x * scaleX
                    # tgtY = dispY + y * scaleY
                    width, height = Image.open(filepath).size
                    SPRITES[filepath] = [len(TEXTURES), 0, 0, 1, 1, width, height];
                    TEXTURES.append(filepath)

                    shutil.copyfile(filepath, os.path.join(outdir, filepath))

    atlas_tex = sorted(atlas_tex, key=lambda i:i.frames[0].perimeter, reverse=True)
    for texture in atlas_tex:
        atlas.pack(texture)

    atlas.write(os.path.join(outdir, "atlas.png"), "RGBA")

    for tex in atlas.textures:
        frame = tex.frames[0]
        SPRITES[tex.name] = [
            0,  # texture id
            1.0 * frame.x / ATLAS_WIDTH,
            1.0 * frame.y / ATLAS_HEIGHT,
            1.0 * frame.width / ATLAS_WIDTH,
            1.0 * frame.height / ATLAS_HEIGHT,
            frame.width,
            frame.height
        ]

    outfile = sys.argv[2]
    with open(outfile, "w") as fout:
        fout.write("TEXTURES = ")
        json.dump(TEXTURES, fout, indent=2)
        fout.write(";\n")
        fout.write("SPRITES = ")
        json.dump(SPRITES, fout, indent=2)
        fout.write(";\n")


if __name__ == "__main__":
    main()
