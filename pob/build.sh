#!/bin/bash
set -e

rm -rf build
mkdir build
cp -r PathOfBuilding build/
pushd build/PathOfBuilding

# replace large files with 0 byte (but they still exist)
for filename in TreeData/{*.jpg,*.png} Assets/* TreeData/{2_6,3_6,3_7,3_8,3_9,3_10,3_11,3_12,3_13,3_14,3_15,3_16,3_17,3_18,3_19}/{*.png,*.jpg}; do
    echo $filename && echo -n > $filename
done

# remove bom
find -iname "*.lua" | xargs sed -i '1s/^\xEF\xBB\xBF//'

# copy in common scripts
cp -r ../../common/* .

popd
