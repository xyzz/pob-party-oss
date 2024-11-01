#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <zlib.h>
#include <dirent.h>
#include <glob.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>

#include <emscripten.h>
#include <emscripten/html5.h>

#include "renderer.h"

double g_last_click;
int g_cursor_x, g_cursor_y;
int g_left_down;
int g_shift_down, g_ctrl_down, g_alt_down;
int g_ctrl_once;
const char *g_paste;
char *g_build;

#define SIZE_DIFF (-2)

typedef struct {
  int js_id;
} image_handle_t;

#define ADDFUNC(n)                                                             \
  lua_pushcclosure(L, l_##n, 0);                                               \
  lua_setglobal(L, #n);
#define ADDFUNCCL(n, u)                                                        \
  lua_pushcclosure(L, l_##n, u);                                               \
  lua_setglobal(L, #n);

static int l_GetCursorPos(lua_State *L) {
  lua_pushinteger(L, g_cursor_x);
  lua_pushinteger(L, g_cursor_y);
  return 2;
}

static int l_OpenURL(lua_State *L) {
  int n = lua_gettop(L);
  const char *url = lua_tostring(L, 1);
  js_OpenURL(url);
  return 0;
}

static int l_DisplayOverlay(lua_State *L) {
  EM_ASM(js_DisplayOverlay(););
  return 0;
}

static int l_SetMainObject(lua_State *L) {
  int n = lua_gettop(L);
  lua_pushstring(L, "MainObject");
  if (n >= 1) {
    lua_pushvalue(L, 1);
  } else {
    lua_pushnil(L);
  }
  lua_settable(L, lua_upvalueindex(1));
  return 0;
}

static int l_Deflate(lua_State *L) {
  int n = lua_gettop(L);
  struct z_stream_s z;
  z.zalloc = NULL;
  z.zfree = NULL;
  deflateInit(&z, 9);
  size_t inLen;
  uint8_t *in = (uint8_t *)lua_tolstring(L, 1, &inLen);
  int outSz = deflateBound(&z, inLen);
  uint8_t *out = malloc(outSz);
  z.next_in = in;
  z.avail_in = inLen;
  z.next_out = out;
  z.avail_out = outSz;
  int err = deflate(&z, Z_FINISH);
  deflateEnd(&z);
  if (err == Z_STREAM_END) {
    lua_pushlstring(L, (const char *)out, z.total_out);
    return 1;
  } else {
    lua_pushnil(L);
    lua_pushstring(L, zError(err));
    return 2;
  }
}

static int l_Inflate(lua_State *L) {
  int n = lua_gettop(L);
  size_t inLen;
  uint8_t *in = (uint8_t *)lua_tolstring(L, 1, &inLen);
  int outSz = inLen * 4;
  uint8_t *out = malloc(outSz);
  struct z_stream_s z;
  z.next_in = in;
  z.avail_in = inLen;
  z.zalloc = NULL;
  z.zfree = NULL;
  z.next_out = out;
  z.avail_out = outSz;
  inflateInit(&z);
  int err;
  while ((err = inflate(&z, Z_NO_FLUSH)) == Z_OK) {
    if (z.avail_out == 0) {
      int newSz = outSz * 2;
      uint8_t *newOut = realloc(out, newSz);
      if (newOut) {
        out = newOut;
      } else {
        return 0;
      }
      z.next_out = out + outSz;
      z.avail_out = outSz;
      outSz = newSz;
    }
  }
  inflateEnd(&z);
  if (err == Z_STREAM_END) {
    lua_pushlstring(L, (const char *)out, z.total_out);
    return 1;
  } else {
    lua_pushnil(L);
    lua_pushstring(L, zError(err));
    return 2;
  }
}

static int l_GetTime(lua_State *L) {
  lua_pushinteger(L, time(NULL));
  return 1;
}

static int l_GetPreciseTime(lua_State *L) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  double res = ts.tv_sec + ts.tv_nsec / 1e9;
  lua_pushnumber(L, res);
  return 1;
}

static int l_IsKeyDown(lua_State *L) {
  int n = lua_gettop(L);
  size_t len;
  const char *kname = lua_tolstring(L, 1, &len);
  int result = 0;
  if (strcmp(kname, "LEFTBUTTON") == 0)
    result = g_left_down;
  else if (strcmp(kname, "CTRL") == 0) {
    result = g_ctrl_down;
    if (g_ctrl_once) {
      result = 1;
      --g_ctrl_once;
    }
  }
  else if (strcmp(kname, "SHIFT") == 0)
    result = g_shift_down;
  else if (strcmp(kname, "ALT") == 0)
    result = g_alt_down;
  else
    printf("Unknown modifier %s\n", kname);
  lua_pushboolean(L, result);
  return 1;
}

static int l_SetDrawLayer(lua_State *L) {
  int n = lua_gettop(L);
  if (lua_isnil(L, 1)) {
    js_SetDrawSubLayer(lua_tointeger(L, 2));
  } else if (n >= 2) {
    js_SetDrawLayer(lua_tointeger(L, 1), lua_tointeger(L, 2));
  } else {
    js_SetDrawLayer(lua_tointeger(L, 1), 0);
  }
  return 0;
}

static int l_SetViewport(lua_State *L) {
  int n = lua_gettop(L);
  if (n) {
    js_SetViewport((int)lua_tointeger(L, 1), (int)lua_tointeger(L, 2), (int)lua_tointeger(L, 3), (int)lua_tointeger(L, 4));
  } else {
    js_ResetViewport();
  }
  return 0;
}

static int l_GetScreenSize(lua_State *L) {
  int width = js_GetWidth();
  int height = js_GetHeight();
  lua_pushinteger(L, width);
  lua_pushinteger(L, height);
  return 2;
}

static const float colorEscape[10][4] = {
    {0.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f},
    {0.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f},
    {1.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 1.0f, 1.0f},
    {0.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f},
    {0.7f, 0.7f, 0.7f, 1.0f}, {0.4f, 0.4f, 0.4f, 1.0f}};

static int IsColorEscape(const char *str) {
  if (str[0] != '^') {
    return 0;
  }
  if (isdigit(str[1])) {
    return 2;
  } else if (str[1] == 'x' || str[1] == 'X') {
    for (int c = 0; c < 6; c++) {
      if (!isxdigit(str[c + 2])) {
        return 0;
      }
    }
    return 8;
  }
  return 0;
}

static void ReadColorEscape(const char *str, float *out) {
  int len = IsColorEscape(str);
  switch (len) {
  case 2:
    out[0] = colorEscape[str[1] - '0'][0];
    out[1] = colorEscape[str[1] - '0'][1];
    out[2] = colorEscape[str[1] - '0'][2];
    break;
  case 8: {
    int xr, xg, xb;
    sscanf(str + 2, "%2x%2x%2x", &xr, &xg, &xb);
    out[0] = xr / 255.0f;
    out[1] = xg / 255.0f;
    out[2] = xb / 255.0f;
  } break;
  }
}

static int l_SetDrawColor(lua_State *L) {
  int n = lua_gettop(L);
  float color[4];
  if (lua_type(L, 1) == LUA_TSTRING) {
    ReadColorEscape(lua_tostring(L, 1), color);
    color[3] = 1.0;
  } else {
    for (int i = 1; i <= 3; i++) {
      color[i - 1] = (float)lua_tonumber(L, i);
    }
    if (n >= 4 && !lua_isnil(L, 4)) {
      color[3] = (float)lua_tonumber(L, 4);
    } else {
      color[3] = 1.0;
    }
  }

  uint32_t rgba = (((uint32_t)(color[3] * 255)) << 24) | (((uint32_t)(color[2] * 255)) << 16) | (((uint32_t)(color[1] * 255)) << 8) | ((uint32_t)(color[0] * 255));
  js_SetDrawColor(rgba);

  return 0;
}

static int l_DrawImage(lua_State *L) {
  int js_id = -1;

  if (!lua_isnil(L, 1)) {
    image_handle_t *handle = lua_touserdata(L, 1);
    js_id = handle->js_id;
  }

  int n = lua_gettop(L) - 1;
  if (n > 8)
    return 0;

  float arg[8] = { 0 };
  for (int i = 0; i < n; ++i)
    arg[i] = lua_tonumber(L, i + 2);
  if (n < 8) {
    arg[4] = 0;
    arg[5] = 0;
    arg[6] = 1;
    arg[7] = 1;
  }

  js_DrawImage(js_id, arg[0], arg[1], arg[2], arg[3], arg[4], arg[5], arg[6], arg[7]);
  return 0;
}

static int l_DrawImageQuad(lua_State *L) {
  int js_id = -1;

  if (!lua_isnil(L, 1)) {
    image_handle_t *handle = lua_touserdata(L, 1);
    js_id = handle->js_id;
  }

  int n = lua_gettop(L) - 1;
  if (n > 16)
    return 0;

  float arg[16] = { 0 };
  for (int i = 0; i < n; ++i)
    arg[i] = lua_tonumber(L, i + 2);
  if (n < 16) {
    arg[8]  = 0; // S0
    arg[9]  = 0; // T0
    arg[10] = 1; // S1
    arg[11] = 0; // T1
    arg[12] = 1; // S2
    arg[13] = 1; // T2
    arg[14] = 0; // S3
    arg[15] = 1; // T3
  }

  js_DrawImageQuad(js_id, arg[0], arg[1], arg[2], arg[3], arg[4], arg[5], arg[6], arg[7],
    arg[8], arg[9], arg[10], arg[11], arg[12], arg[13], arg[14], arg[15]);

  return 0;
}

static int l_DrawStringWidth(lua_State *L) {
  lua_pushinteger(L, js_DrawStringWidth(lua_tointeger(L, 1) + SIZE_DIFF, lua_tostring(L, 2), lua_tostring(L, 3)));

  return 1;
}

static int l_DrawStringCursorIndex(lua_State *L) {
  lua_pushinteger(L, js_DrawStringCursorIndex(lua_tointeger(L, 1) + SIZE_DIFF, lua_tostring(L, 2), lua_tostring(L, 3), lua_tointeger(L, 4), lua_tointeger(L, 5)));

  return 1;
}

static int l_DrawString(lua_State *L) {
  js_DrawString(lua_tointeger(L, 1), lua_tointeger(L, 2), lua_tostring(L, 3), lua_tointeger(L, 4) + SIZE_DIFF,
    lua_tostring(L, 5), lua_tostring(L, 6));
  return 0;
}

static int l_NewImageHandle(lua_State *L) {
  image_handle_t *handle = lua_newuserdata(L, sizeof(image_handle_t));
  handle->js_id = js_NewImageHandle();
  lua_pushvalue(L, lua_upvalueindex(1));
  lua_setmetatable(L, -2);
  return 1;
}

static image_handle_t *GetImgHandle(lua_State *L, const char *method,
                                 int loaded) {
  image_handle_t *handle = lua_touserdata(L, 1);
  lua_remove(L, 1);
  return handle;
}

static int l_imgHandleGC(lua_State *L) {
  return 0;
}

static int l_imgHandleLoad(lua_State *L) {
  image_handle_t *handle = GetImgHandle(L, "Load", 0);
  int n = lua_gettop(L);
  const char *filename = lua_tostring(L, 1);
  js_imgHandleLoad(handle->js_id, filename);
  return 0;
}

static int l_imgHandleUnload(lua_State *L) {
  return 0;
}

static int l_imgHandleIsValid(lua_State *L) {
  image_handle_t *imgHandle = GetImgHandle(L, "IsValid", 0);
  lua_pushboolean(L, 1);
  return 1;
}

static int l_imgHandleIsLoading(lua_State *L) {
  image_handle_t *handle = GetImgHandle(L, "IsLoading", 1);
  lua_pushboolean(L, js_imgHandleIsLoading(handle->js_id));
  return 1;
}

static int l_imgHandleSetLoadingPriority(lua_State *L) {
  return 0;
}

static int l_imgHandleImageSize(lua_State *L) {
  image_handle_t *handle = GetImgHandle(L, "ImageSize", 1);
  lua_pushinteger(L, js_imageWidth(handle->js_id));
  lua_pushinteger(L, js_imageHeight(handle->js_id));
  return 2;
}

static int l_MakeDir(lua_State *L) {
  const char *path = lua_tostring(L, 1);
  mkdir(path, 0777);
  lua_pushboolean(L, 1);
  return 1;
}

static int l_RemoveDir(lua_State *L) {
  const char *path = lua_tostring(L, 1);
  rmdir(path);
  lua_pushboolean(L, 1);
  return 1;
}

static int l_StripEscapes(lua_State *L) {
  int n = lua_gettop(L);
  const char *str = lua_tostring(L, 1);
  char *strip = malloc(strlen(str) + 1);
  char *p = strip;
  while (*str) {
    int esclen = IsColorEscape(str);
    if (esclen) {
      str += esclen;
    } else {
      *(p++) = *(str++);
    }
  }
  *p = 0;
  lua_pushstring(L, strip);
  free(strip);
  return 1;
}

// ==============
// Search Handles
// ==============

struct search_handle_t {
  glob_t data;
  size_t index;
  int want_dirs;
};

int is_regular_file(const char *path) {
  struct stat path_stat;
  stat(path, &path_stat);
  return S_ISREG(path_stat.st_mode);
}

static void next_file(struct search_handle_t *handle) {
  while (1) {
    handle->index++;
    if (handle->index >= handle->data.gl_pathc)
      return;
    printf("check %s\n", handle->data.gl_pathv[handle->index]);
    if (is_regular_file(handle->data.gl_pathv[handle->index]) != handle->want_dirs)
      break;
  }
}

static int l_NewFileSearch(lua_State *L) {
  int n = lua_gettop(L);
  const char *pattern = lua_tostring(L, 1);
  printf("NewFileSearch %s\n", pattern);
  int dir_only = lua_toboolean(L, 2);

  glob_t temp;
  glob(pattern, 0, NULL, &temp);

  int match_cnt = 0;
  for (size_t i = 0; i < temp.gl_pathc; ++i) {
    int is_dir = !is_regular_file(temp.gl_pathv[i]);
    if (is_dir == dir_only)
      ++match_cnt;
  }

  if (!match_cnt) {
    globfree(&temp);
    return 0;
  }

  struct search_handle_t *handle = lua_newuserdata(L, sizeof(*handle));
  handle->index = -1;
  handle->want_dirs = dir_only;
  memcpy(&handle->data, &temp, sizeof(temp));

  next_file(handle);

  lua_pushvalue(L, lua_upvalueindex(1));
  lua_setmetatable(L, -2);
  return 1;
}

static int l_searchHandleGC(lua_State *L) {
  struct search_handle_t *handle = lua_touserdata(L, 1);
  globfree(&handle->data);
  return 0;
}

static int l_searchHandleNextFile(lua_State *L) {
  struct search_handle_t *handle = lua_touserdata(L, 1);

  next_file(handle);

  if (handle->index >= handle->data.gl_pathc)
    return 0;

  lua_pushboolean(L, 1);
  return 1;
}

static int l_searchHandleGetFileName(lua_State *L) {
  struct search_handle_t *handle = lua_touserdata(L, 1);

  const char *s = handle->data.gl_pathv[handle->index];
  const char *last_slash = strrchr(s, '/');
  if (last_slash)
    s = last_slash + 1;

  lua_pushstring(L, s);
  return 1;
}

static int l_searchHandleGetFileSize(lua_State *L) {
  lua_pushinteger(L, 1234);
  return 1;
}

static int l_searchHandleGetFileModifiedTime(lua_State *L) {
  lua_pushnumber(L, 4321);
  lua_pushstring(L, "date");
  lua_pushstring(L, "time");
  return 3;
}

static int l_Copy(lua_State *L) {
  int n = lua_gettop(L);
  const char *s = lua_tostring(L, 1);
  js_CopyToClipboard(s);
  return 0;
}

static int l_Paste(lua_State *L) {
  if (g_paste && strlen(g_paste)) {
    lua_pushstring(L, g_paste);
    return 1;
  } else {
    return 0;
  }
}

static lua_State *L;

static void main_loop(void) {
  js_StartFrame();

  lua_getfield(L, LUA_REGISTRYINDEX, "uicallbacks");
  lua_getfield(L, -1, "MainObject");
  lua_getfield(L, -1, "OnFrame");
  lua_insert(L, -2);
  int result = lua_pcall(L, 1, 0, 0);
  if (result != 0)
    lua_error(L);

  js_EndFrame();
}

static void update_modifiers_mouse(const EmscriptenMouseEvent *mouseEvent) {
  g_shift_down = mouseEvent->shiftKey;
  g_ctrl_down = mouseEvent->ctrlKey;
  g_alt_down = mouseEvent->altKey;
}

static EM_BOOL mouse_callback(int eventType, const EmscriptenMouseEvent *mouseEvent, void *userData) {
  g_cursor_x = mouseEvent->targetX;
  g_cursor_y = mouseEvent->targetY;

  update_modifiers_mouse(mouseEvent);

  if (eventType == EMSCRIPTEN_EVENT_MOUSEDOWN) {
    g_left_down = 1;
    lua_getfield(L, LUA_REGISTRYINDEX, "uicallbacks");
    lua_getfield(L, -1, "MainObject");
    lua_getfield(L, -1, "OnKeyDown");
    lua_insert(L, -2);
    lua_pushstring(L, "LEFTBUTTON");

    int double_click = (emscripten_get_now() - g_last_click) < 200; // 200ms

    lua_pushboolean(L, double_click);
    int result = lua_pcall(L, 3, 0, 0);
    if (result != 0)
      lua_error(L);
  } else if (eventType == EMSCRIPTEN_EVENT_MOUSEUP) {
    g_left_down = 0;
    lua_getfield(L, LUA_REGISTRYINDEX, "uicallbacks");
    lua_getfield(L, -1, "MainObject");
    lua_getfield(L, -1, "OnKeyUp");
    lua_insert(L, -2);
    lua_pushstring(L, "LEFTBUTTON");
    int result = lua_pcall(L, 2, 0, 0);
    if (result != 0)
      lua_error(L);

    g_last_click = emscripten_get_now();
  }

  return 1;
}

float g_last_wheel;
float g_wheel_sum;

static EM_BOOL wheel_callback(int eventType, const EmscriptenWheelEvent *wheelEvent, void *userData) {
  float diff = wheelEvent->deltaY;
  if (wheelEvent->deltaMode == DOM_DELTA_LINE)
    diff *= 10;
  if (wheelEvent->deltaMode == DOM_DELTA_PAGE)
    diff *= 1000;

  // reset cumulative wheel
  if (emscripten_get_now() - g_last_wheel > 100) {
    g_wheel_sum = 0;
  }
  g_last_wheel = emscripten_get_now();

  g_wheel_sum += diff;

  lua_getfield(L, LUA_REGISTRYINDEX, "uicallbacks");
  lua_getfield(L, -1, "MainObject");
  lua_getfield(L, -1, "OnKeyUp");
  lua_insert(L, -2);
  if (g_wheel_sum > 50) {
    lua_pushstring(L, "WHEELDOWN");
    g_wheel_sum = 0;
  } else if (g_wheel_sum < -50) {
    lua_pushstring(L, "WHEELUP");
    g_wheel_sum = 0;
  } else {
    return 1;
  }
  lua_pushboolean(L, 0);
  int result = lua_pcall(L, 3, 0, 0);
  if (result != 0) {
    lua_error(L);
  }

  return 1;
}

static void send_key(const char *key, int is_up) {
  lua_getfield(L, LUA_REGISTRYINDEX, "uicallbacks");
  lua_getfield(L, -1, "MainObject");
  lua_getfield(L, -1, is_up ? "OnKeyUp" : "OnKeyDown");
  lua_pushstring(L, key);
  lua_insert(L, -2);
  lua_insert(L, -3);
  lua_pushboolean(L, 0);
  int result = lua_pcall(L, 3, 0, 0);
  if (result != 0) {
    lua_error(L);
  }
}

struct mapper {
  const char *em;
  const char *pob;
};

struct mapper key_mappings[] = {
  { "Backspace", "BACK" },
  { "Escape", "ESCAPE" },
  { "Enter", "RETURN" },
  { "ArrowLeft", "LEFT" },
  { "ArrowRight", "RIGHT" },
  { "ArrowUp", "UP" },
  { "ArrowDown", "DOWN" },
  { "PageUp", "PAGEUP" },
  { "PageDown", "PAGEDOWN" },
  { "Delete", "DELETE" },
  { "Home", "HOME" },
  { "End", "END" },
};

static void handle_special_keys(const char *code, int is_up) {
  for (size_t i = 0; i < sizeof(key_mappings)/sizeof(*key_mappings); ++i) {
    if (strcmp(code, key_mappings[i].em) == 0) {
      send_key(key_mappings[i].pob, is_up);
      return;
    }
  }
}

static void update_modifiers(const EmscriptenKeyboardEvent *keyEvent) {
  g_shift_down = keyEvent->shiftKey;
  g_ctrl_down = keyEvent->ctrlKey;
  g_alt_down = keyEvent->altKey;
}

static EM_BOOL keydown_callback(int eventType, const EmscriptenKeyboardEvent *keyEvent, void *userData) {
  // Special-case ctrl-v
  if (g_ctrl_down && strcmp(keyEvent->key, "v") == 0)
    return 0;

  update_modifiers(keyEvent);
  if (keyEvent->key[0] != '\0' && keyEvent->key[1] == '\0') {
    if (g_ctrl_down) {
      // if ctrl is held, send an OnKeyDown instead of OnChar
      send_key(keyEvent->key, 0);
    } else {
      lua_getfield(L, LUA_REGISTRYINDEX, "uicallbacks");
      lua_getfield(L, -1, "MainObject");
      lua_getfield(L, -1, "OnChar");
      lua_pushstring(L, keyEvent->key);

      lua_insert(L, -2);
      lua_insert(L, -3);
      lua_pushboolean(L, 0);
      int result = lua_pcall(L, 3, 0, 0);
      if (result != 0) {
        lua_error(L);
      }
    }
  } else {
    handle_special_keys(keyEvent->code, 0);
  }

  return 1;
}

static EM_BOOL keyup_callback(int eventType, const EmscriptenKeyboardEvent *keyEvent, void *userData) {
  update_modifiers(keyEvent);

  if (keyEvent->key[0] != '\0' && keyEvent->key[1] == '\0') {
    send_key(keyEvent->key, 1);
  } else {
    handle_special_keys(keyEvent->code, 1);
  }

  return 1;
}

void inject_paste(const char *str) {
  if (g_paste)
    free(g_paste);
  g_paste = str;

  g_ctrl_once = 1;
  send_key("v", 0);
  send_key("v", 1);
}

const char* generate_build(void) {
  int result;

  lua_getfield(L, LUA_REGISTRYINDEX, "uicallbacks");
  lua_getfield(L, -1, "MainObject");
  lua_getfield(L, -1, "GenerateBuild");
  lua_insert(L, -2);
  result = lua_pcall(L, 1, 1, 0);
  if (result != 0)
    lua_error(L);

  const char *build = lua_tostring(L, -1);

  if (g_build)
    free(g_build);
  g_build = strdup(build);

  lua_pop(L, 1);

  return g_build;
}

static void import_build(const char *build) {
  lua_getfield(L, LUA_REGISTRYINDEX, "uicallbacks");
  lua_getfield(L, -1, "MainObject");
  lua_getfield(L, -1, "ImportBuild");
  lua_insert(L, -2);
  lua_pushstring(L, build);

  int result = lua_pcall(L, 2, 0, 0);
  if (result != 0)
    lua_error(L);
}

void profiler_start(void) {
  lua_getglobal(L, "profiler_start");
  if (lua_pcall(L, 0, 0, 0) != 0)
    lua_error(L);
}

void profiler_stop(void) {
  lua_getglobal(L, "profiler_stop");
  if (lua_pcall(L, 0, 0, 0) != 0)
    lua_error(L);
}

int main(int argc, char *argv[]) {
  L = luaL_newstate();
  luaL_openlibs(L);

  // Callbacks
  lua_newtable(L);      // Callbacks table
  lua_pushvalue(L, -1); // Push callbacks table
  ADDFUNCCL(SetMainObject, 1);
  lua_setfield(L, LUA_REGISTRYINDEX, "uicallbacks");

  // Image handles
  lua_newtable(L);      // Image handle metatable
  lua_pushvalue(L, -1); // Push image handle metatable
  ADDFUNCCL(NewImageHandle, 1);
  lua_pushvalue(L, -1); // Push image handle metatable
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, l_imgHandleGC);
  lua_setfield(L, -2, "__gc");
  lua_pushcfunction(L, l_imgHandleLoad);
  lua_setfield(L, -2, "Load");
  lua_pushcfunction(L, l_imgHandleUnload);
  lua_setfield(L, -2, "Unload");
  lua_pushcfunction(L, l_imgHandleIsValid);
  lua_setfield(L, -2, "IsValid");
  lua_pushcfunction(L, l_imgHandleIsLoading);
  lua_setfield(L, -2, "IsLoading");
  lua_pushcfunction(L, l_imgHandleSetLoadingPriority);
  lua_setfield(L, -2, "SetLoadingPriority");
  lua_pushcfunction(L, l_imgHandleImageSize);
  lua_setfield(L, -2, "ImageSize");
  lua_setfield(L, LUA_REGISTRYINDEX, "uiimghandlemeta");

  // Rendering
  ADDFUNC(GetScreenSize);
  ADDFUNC(SetDrawLayer);
  ADDFUNC(SetViewport);
  ADDFUNC(SetDrawColor);
  ADDFUNC(DrawImage);
  ADDFUNC(DrawImageQuad);
  ADDFUNC(DrawString);
  ADDFUNC(DrawStringWidth);
  ADDFUNC(DrawStringCursorIndex);
  ADDFUNC(StripEscapes);

  // Search handles
  lua_newtable(L);      // Search handle metatable
  lua_pushvalue(L, -1); // Push search handle metatable
  ADDFUNCCL(NewFileSearch, 1);
  lua_pushvalue(L, -1); // Push search handle metatable
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, l_searchHandleGC);
  lua_setfield(L, -2, "__gc");
  lua_pushcfunction(L, l_searchHandleNextFile);
  lua_setfield(L, -2, "NextFile");
  lua_pushcfunction(L, l_searchHandleGetFileName);
  lua_setfield(L, -2, "GetFileName");
  lua_pushcfunction(L, l_searchHandleGetFileSize);
  lua_setfield(L, -2, "GetFileSize");
  lua_pushcfunction(L, l_searchHandleGetFileModifiedTime);
  lua_setfield(L, -2, "GetFileModifiedTime");
  lua_setfield(L, LUA_REGISTRYINDEX, "uisearchhandlemeta");

  // General function
  ADDFUNC(GetCursorPos);
  ADDFUNC(IsKeyDown);
  ADDFUNC(Copy);
  ADDFUNC(Paste);
  ADDFUNC(Deflate);
  ADDFUNC(Inflate);
  ADDFUNC(GetTime);
  ADDFUNC(GetPreciseTime);
  ADDFUNC(MakeDir);
  ADDFUNC(RemoveDir);
  ADDFUNC(OpenURL);
  ADDFUNC(DisplayOverlay);

  chdir("PathOfBuilding");

  int result = luaL_dofile(L, "wrapper.lua");
  if (result != 0)
    lua_error(L);

  lua_getfield(L, LUA_REGISTRYINDEX, "uicallbacks");
  lua_getfield(L, -1, "MainObject");
  lua_getfield(L, -1, "OnInit");
  lua_insert(L, -2);
  result = lua_pcall(L, 1, 0, 0);
  if (result != 0)
    lua_error(L);

  emscripten_set_mousedown_callback("#glCanvas", NULL, 1, mouse_callback);
  emscripten_set_mouseup_callback("#glCanvas", NULL, 1, mouse_callback);
  emscripten_set_mousemove_callback("#glCanvas", NULL, 1, mouse_callback);

  emscripten_set_wheel_callback("#glCanvas", NULL, 1, wheel_callback);

  emscripten_set_keydown_callback("body", NULL, 1, keydown_callback);
  emscripten_set_keyup_callback("body", NULL, 1, keyup_callback);

  EM_ASM(document.getElementById("loading").outerHTML = "";);

  if (argc == 2) {
    main_loop();
    const char *build = argv[1];
    import_build(build);
  }

  emscripten_set_main_loop(main_loop, 0, 0);

  return 0;
}
