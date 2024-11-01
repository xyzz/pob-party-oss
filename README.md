# pob-web

## Description

Run [PathOfBuilding](https://github.com/Openarl/PathOfBuilding) in your browser.

## Build

First, download and install [emscripten](https://kripken.github.io/emscripten-site/).

Before proceeding, make sure you're operating in an emscripten environment (`source ./emsdk_env.sh`), i.e. `emcc` command is available.

### Build
```
make clean && make -j8
```

## Run

The output is located in the `output` directory, you can serve it with any webserver, e.g.

```
cd output
python3 -m http.server
```

## Updating tree

All steps below have to be done to update to latest version of PoB.

### Updating from github

Change whatever in `~/pob`. Specifically: fetch upstream, then rebase web on top of it. Force-push to github, it's fine because a "real" copy of pob lives on in *this* repo.

```
cd ~/pob
git fetch LocalIdentity
git rebase LocalIdentity/master
# fix conflicts and
git push --force
```

Run `./tool/copy_pob.sh` to copy in the new changes.

### Updating the tree data/statics

NB: this is no longer needed when using LocalIdentity's fork.

Run pob on windows and update to latest. Then, copy `TreeData` and `manifest.xml` from windows-pob into `pob/PathOfBuilding`. It's located in `C:\ProgramData\Path of Building`.

### Changing game version

If the game has been updated to a new version (e.g. `v3.6.0` to `v3.7.0`):
- create a new branch for the current version (`v3.6.0`)
- back on the master branch, update `tool/assets.py`, `pob/build.sh` (e.g `3_6` => `3_7`)
- change `CURRENT_VERSION` in the Makefile to `v3.7.0`
- update `src/versions.html`
- commit & push everything
- run full-build.sh and then upload.sh
- add new version (`v3.7.0`) to the end of `poe/pob-kv/main.py`'s' `ALLOWED_VER` & redeploy & **manually restart** with `supervisorctl restart pob` on db server
