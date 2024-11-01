#!/bin/bash

set -e

pushd pob
rm -rf old
cp -r PathOfBuilding old
rm -rf PathOfBuilding && mkdir PathOfBuilding
pushd PathOfBuilding
mkdir -p src
cp -r ../../../pob/src/{Assets,Classes,Data,Launch.lua,Modules,UpdateApply.lua,UpdateCheck.lua,GameVersions.lua,TreeData} .
cp ../../../pob/manifest.xml .
rm -rf base64.lua dkjson.lua lcurl sha1.lua xml.lua
echo "" > changelog.txt
popd
rm -rf old
popd
