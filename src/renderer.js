mergeInto(LibraryManager.library, {
	js_StartFrame: function() {
		draw.StartFrame();
	},
	js_EndFrame: function() {
		draw.EndFrame();
	},

	js_DrawImageQuad: function(id, x0, y0, x1, y1, x2, y2, x3, y3, s0, t0, s1, t1, s2, t2, s3, t3) {
		if (id < 0 || (image_handles.hasOwnProperty(id) && image_handles[id].sprite))
			draw.PushCmd(draw.p_DrawImageQuad, [id, x0, y0, x1, y1, x2, y2, x3, y3, s0, t0, s1, t1, s2, t2, s3, t3]);
	},
	js_DrawImage: function(id, x0, y0, w, h, s1, t1, s2, t2) {
		_js_DrawImageQuad(id, x0, y0, x0+w, y0, x0+w, y0+h, x0, y0+h, s1, t1, s2, t1, s2, t2, s1, t2);
	},
	js_SetDrawColor: function(rgba) {
		rgba = rgba >>> 0;
		idx_by_color(rgba);
		draw.PushCmd(draw.p_SetDrawColor, [rgba]);
	},
	js_SetViewport: function(x, y, w, h) {
		draw.PushCmd(draw.p_SetViewport, [x, y, w, h]);
	},
	js_ResetViewport: function() {
		_js_SetViewport(0, 0, glCanvas.width, glCanvas.height);
	},
	js_DrawString: function(x, y, align, size, font, s) {
		align = UTF8ToString(align);
		font = UTF8ToString(font);
		s = UTF8ToString(s);

		draw.PushCmd(draw.p_DrawString, [x, y, align, size, font, s]);
	},
	js_DrawStringWidth: function(size, font, text) {
		font = UTF8ToString(font);
		text = UTF8ToString(text);

		return draw.DrawStringWidth(size, font, text);
	},
	js_DrawStringCursorIndex: function(size, font, text, x, y) {
		font = UTF8ToString(font);
		text = UTF8ToString(text);

		return draw.DrawStringCursorIndex(size, font, text, x, y);
	},
	js_SetDrawSubLayer: function(sublayer) {
		draw.SetDrawSubLayer(sublayer);
	},
	js_SetDrawLayer: function(layer, sublayer) {
		draw.SetDrawLayer(layer, sublayer);
	},

	js_NewImageHandle: function() {
		var handle = {};

		handle.loaded = false;
		handle.width = 1;
		handle.height = 1;

		image_handles.push(handle);
		return image_handles.length - 1;
	},
	js_imgHandleLoad: function(id, filename) {
		filename = UTF8ToString(filename);
		filename = "PathOfBuilding/" + filename;

		var handle = image_handles[id];
		if (!SPRITES.hasOwnProperty(filename)) {
			console.log(filename + " - not found");
			return;
		}
		var x = SPRITES[filename];
		handle.width = x[5];
		handle.height = x[6];
		handle.loaded = true;
		handle.sprite = x;
	},
	js_imgHandleIsLoading: function(id) {
		return !image_handles[id].loaded;
	},
	js_imageWidth: function(id) {
		return image_handles[id].width;
	},
	js_imageHeight: function(id) {
		return image_handles[id].height;
	},

	js_GetWidth: function() {
		return glCanvas.width;
	},
	js_GetHeight: function() {
		return glCanvas.height;
	},

	js_CopyToClipboard: function(s) {
		s = UTF8ToString(s);

		var el = document.createElement('textarea');
		el.value = s;
		document.body.appendChild(el);
		el.select();
		document.execCommand('copy');
		document.body.removeChild(el);
	},

	js_OpenURL: function(s) {
		s = UTF8ToString(s);
		window.open(s, '_blank');
	}
});
