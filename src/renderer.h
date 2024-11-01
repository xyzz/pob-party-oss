#pragma once

extern void js_StartFrame(void);
extern void js_EndFrame(void);

extern void js_DrawImageQuad(int hid, float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3,
	float s0, float t0, float s1, float t1, float s2, float t2, float s3, float t3);
extern void js_DrawImage(int hid, float x0, float y0, float w, float h, float tcLeft, float tcTop, float tcRight, float tcBottom);

extern int js_NewImageHandle(void);
extern void js_imgHandleLoad(int h, const char *path);
extern int js_imgHandleIsLoading(int h);
extern int js_imageWidth(int h);
extern int js_imageHeight(int h);

extern void js_DrawString(int x, int y, const char *align, int size, const char *font, const char *s);
extern int js_GetWidth(void);
extern int js_GetHeight(void);

extern void js_SetDrawColor(unsigned rgba);
extern void js_SetViewport(int x, int y, int w, int h);
extern void js_ResetViewport(void);
extern void js_SetDrawLayer(int layer, int sublayer);
extern void js_SetDrawSubLayer(int sublayer);
extern int js_DrawStringWidth(int size, const char *font, const char *text);
extern int js_DrawStringCursorIndex(int size, const char *font, const char *text, int x, int y);

extern void js_CopyToClipboard(const char *s);

extern void js_OpenURL(const char *s);
