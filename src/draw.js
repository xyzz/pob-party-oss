COLOR_MAP = [
  "#000000",
  "#FF0000",
  "#00FF00",
  "#0000FF",
  "#FFFFFF",
  "#FF00FF",
  "#00FFFF",
  "#FFFFFF",
  "#B2B2B2",
  "#666666"
];

function getPowerOfTwo(value, pow) {
	var pow = pow || 1;
	while(pow<value) {
		pow *= 2;
	}
	return pow;
}

function getFont(font) {
	if (font == "FIXED") {
		font = "BitstreamVeraSansMono";
	} else {
		font = "LiberationSans";
	}

	return font;
}

function intcmp(a, b) {
	return a - b;
}

function oversized(text) {
	var sanitized = text.replace(/\^x.{6}/gi, "").replace(/\^./gi, "");
	if (sanitized.length > 2000)
		return "[oversized text]";
	return text;
}

function getGpu() {
	var ext = gl.getExtension("WEBGL_debug_renderer_info");
	if (!ext) {
		return "Unknow";
	}
	return gl.getParameter(ext.UNMASKED_RENDERER_WEBGL);
}

cachedText = {
	cache: {},
	widthCache: {},
	canvas: document.getElementById('textCanvas'),
	ctx: document.getElementById('textCanvas').getContext("2d"),

	getTextureForString: function(size, font, text) {
		var hash = size + ":" + font + ":" + text;
		if (this.cache.hasOwnProperty(hash))
			return this.cache[hash];
		this.ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);

		var font_style = size + "px " + font;

		this.ctx.font = font_style;

		var lines = text.split("\n");
		var w = 0;
		var h = size * (lines.length + 1);
		for (var i = 0; i < lines.length; ++i)
			w = Math.max(w, this.ctx.measureText(lines[i]).width);

		this.canvas.width = getPowerOfTwo(w);
		this.canvas.height = getPowerOfTwo(h);

		// display border for debugging
		// this.ctx.strokeStyle = "cyan";
		// this.ctx.strokeRect(0, 0, w, h);

		this.ctx.font = font_style;
		this.ctx.fillStyle = "white";
		for (var i = 0; i < lines.length; i++)
			this.ctx.fillText(lines[i], 0, size * (i + 1));

		var tex = gl.createTexture();
		gl.bindTexture(gl.TEXTURE_2D, tex);
		gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, this.canvas);
		gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
		gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR_MIPMAP_NEAREST);
		gl.generateMipmap(gl.TEXTURE_2D);

		var obj = {
			tex: tex,
			w: w,
			h: h,
			tw: w / this.canvas.width,
			th: h / this.canvas.height
		};

		this.cache[hash] = obj;
		return obj;
	}
};

function recreatePalette() {
	console.log("Recreating palette with " + draw.palette_colors_ordered.length + " colors");

	// Setup a palette.
	var palette = new Uint8Array(256 * 4);

	// I'm lazy so just setting 4 colors in palette
	function setPalette(index, r, g, b, a) {
	    palette[index * 4 + 0] = r;
	    palette[index * 4 + 1] = g;
	    palette[index * 4 + 2] = b;
	    palette[index * 4 + 3] = a;
	}
	
	for (var i = 0; i < draw.palette_colors_ordered.length; ++i) {
		var color = draw.palette_colors_ordered[i];
		setPalette(i + 1, color % 256, (color >>> 8) % 256, (color >>> 16) % 256, (color >>> 24));
		draw.palette_idx_by_color[color] = i + 1;
	}

	// make palette texture and upload palette
	gl.activeTexture(gl.TEXTURE1);
	var paletteTex = gl.createTexture();
	gl.bindTexture(gl.TEXTURE_2D, paletteTex);
	gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
	gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
	gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
	gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
	gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 256, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE, palette);

	gl.activeTexture(gl.TEXTURE0);
}

function idx_by_color(rgba) {
	var idx = draw.palette_idx_by_color[rgba];

	if (idx !== undefined)
		return idx;

	if (draw.palette_colors[rgba] != 1) {
		draw.palette_colors[rgba] = 1;
		draw.palette_colors_ordered.push(rgba);
	}

	recreatePalette();
	return idx_by_color(rgba);
}

function round4(f) {
	return Math.round(f * 10000.0) / 10000.0;
}

var MAXV = 30000;
var gbuf32 = new Float32Array(12 * MAXV);
var gbuf8 = new Uint8Array(6 * MAXV);

draw = {
	palette_colors: {},
	palette_colors_ordered: [],
	palette_idx_by_color: {},
	firstRun: true,

	StartFrame: function() {
		draw.cmds = {};
		draw.color = 0xFFFFFFFF;
		draw.viewport = {
			x: 0,
			y: 0,
			w: glCanvas.width,
			h: glCanvas.height
		};
		draw.layer = 0;
		draw.sublayer = 0;
		draw.cnt = 0;
		draw.quad_calls = 0;
		draw.multi_draws = 0;
		draw.set_viewport = 0;

		gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
		resetBuffer();
		gl.useProgram(shaderProgram);

		gl.enableVertexAttribArray(vertexPositionAttribute);
		gl.enableVertexAttribArray(texcoordLocation);
		gl.enableVertexAttribArray(colorLocation1);
		gl.uniform1i(textureLocation, 0);
		gl.uniform1i(paletteLocation, 1);

		this.p_SetViewport(0, 0, glCanvas.width, glCanvas.height);

		draw.ts_start = performance.now();

		if (draw.firstRun) {
			recreatePalette();
			draw.gpu = getGpu();

			draw.firstRun = false;
		}
	},
	EndFrame: function() {
		draw.ts_native = performance.now();

		var layerKeys = Object.keys(this.cmds).sort(intcmp);
		var cmds = [];
		for (var i = 0; i < layerKeys.length; ++i) {
			var x = layerKeys[i];
			var sublayerKeys = Object.keys(this.cmds[x]).sort(intcmp);
			for (var j = 0; j < sublayerKeys.length; ++j) {
				var y = sublayerKeys[j];
				for (var k = 0; k < this.cmds[x][y].length; ++k) {
					cmds.push(draw.cmds[x][y][k]);
				}
			}
		}

		draw.ts_optimize = performance.now();

		var cnt = 0;
		for (var start = 0; start < cmds.length; ++start) {
			var ourtex = -2;
			cnt++;

			var end = start;
			while (end < cmds.length && (cmds[end][0] == draw.p_DrawImageQuad || cmds[end][0] == draw.p_SetDrawColor)) {
				if (cmds[end][0] == draw.p_DrawImageQuad) {
					var texid = cmds[end][1][0];
					if (texid >= 0)
						texid = image_handles[texid].sprite[0];
					if (ourtex == -2)
						ourtex = texid;
					if (texid != ourtex) 
						break;
				}

				++end;
			}
			if (start == end) {
				/// single draw
				cmds[start][0].apply(this, cmds[start][1]);
			} else {
				/// instanced draw
				draw.InstancedDrawQuads(cmds, start, end);
				start = end - 1;
			}
		}

		draw.ts_end = performance.now();

		if (document.getElementById("debug").style.display != "block")
			return;

		var ts_all = draw.ts_end - draw.ts_start;
		var fps = 1000.0 / ts_all;

		document.getElementById("debug-contents").innerHTML =
			"GPU: " + draw.gpu +
			"<br>Native: " + round4(draw.ts_native - draw.ts_start) +
			"<br>Optimize: " + round4(draw.ts_optimize - draw.ts_native) +
			"<br>Render: " + round4(draw.ts_end - draw.ts_optimize) +
			"<br>All: " + round4(ts_all) +
			"<br>FPS: " + round4(fps) +
			"<br>Commands: " + draw.cnt +
			"<br>Single draws: " + draw.quad_calls +
			"<br>Multi draws: " + draw.multi_draws +
			"<br>Viewport switches: " + draw.set_viewport +
			"<br>Palette size: " + Object.keys(draw.palette_colors).length +
			"<br>Array buffer size: " + round4(gl.bufferOffset / 1024) + " KiB" +
			"";
	},
	PushCmd: function(cmd, args) {
		draw.cnt++;
		if (!draw.cmds.hasOwnProperty(draw.layer))
			draw.cmds[draw.layer] = {};
		if (!draw.cmds[draw.layer].hasOwnProperty(draw.sublayer))
			draw.cmds[draw.layer][draw.sublayer] = [];
		draw.cmds[draw.layer][draw.sublayer].push([cmd, args]);
	},

	InstancedDrawQuads: function(cmds, start, end) {
		var color = idx_by_color(draw.color);
		var draws = 0;

		var texture;

		for (var i = start; i < end; ++i) {
			var func = cmds[i][0];
			if (func == draw.p_DrawImageQuad) {
				texture = cmds[i][1][0];
				++draws;
			}
		}

		var tex;
		if (texture >= 0) {
			tex = image_handles[texture].sprite[0];
		} else {
			tex = whiteTexture;
		}
		gl.bindTexture(gl.TEXTURE_2D, tex);

		var dstStart = 0;
		var dstCur = dstStart;
		var texStart = ~~(12 * MAXV / 2);
		var texCur = texStart;
		var colStart = 0;
		var colCur = colStart;

		for (var i = start; i < end; ++i) {
			var func = cmds[i][0];
			var args = cmds[i][1];
			if (func == draw.p_SetDrawColor) {
				draw.color = args[0];
				color = idx_by_color(draw.color);
				continue;
			}

			var sp;
			if (image_handles[args[0]] !== undefined) {
				sp = image_handles[args[0]].sprite;
			} else {
				sp = [-1, 0, 0, 1, 1];
			}

			gbuf32[dstCur++] = args[1];
			gbuf32[dstCur++] = args[2];
			gbuf32[dstCur++] = args[3];
			gbuf32[dstCur++] = args[4];
			gbuf32[dstCur++] = args[5];
			gbuf32[dstCur++] = args[6];
			gbuf32[dstCur++] = args[1];
			gbuf32[dstCur++] = args[2];
			gbuf32[dstCur++] = args[5];
			gbuf32[dstCur++] = args[6];
			gbuf32[dstCur++] = args[7];
			gbuf32[dstCur++] = args[8];

			gbuf32[texCur++] = sp[1] + sp[3] * args[9];
			gbuf32[texCur++] = sp[2] + sp[4] * args[10];
			gbuf32[texCur++] = sp[1] + sp[3] * args[11];
			gbuf32[texCur++] = sp[2] + sp[4] * args[12];
			gbuf32[texCur++] = sp[1] + sp[3] * args[13];
			gbuf32[texCur++] = sp[2] + sp[4] * args[14];
			gbuf32[texCur++] = sp[1] + sp[3] * args[9];
			gbuf32[texCur++] = sp[2] + sp[4] * args[10];
			gbuf32[texCur++] = sp[1] + sp[3] * args[13];
			gbuf32[texCur++] = sp[2] + sp[4] * args[14];
			gbuf32[texCur++] = sp[1] + sp[3] * args[15];
			gbuf32[texCur++] = sp[2] + sp[4] * args[16];

			gbuf8[colCur++] = color;
			gbuf8[colCur++] = color;
			gbuf8[colCur++] = color;
			gbuf8[colCur++] = color;
			gbuf8[colCur++] = color;
			gbuf8[colCur++] = color;
		}

		dstCur += 8 - (dstCur % 8);
		texCur += 8 - (texCur % 8);
		colCur += 4 - (colCur % 4);

		if (dstCur >= texStart)
			throw "vertex buffer internal overflow with draws=" + draws + " dstCur=" + dstCur + " texStart=" + texStart;

		if (draws == 0)
			return;

		gl.bufferSubData(gl.ARRAY_BUFFER, gl.bufferOffset, gbuf32, dstStart, dstCur - dstStart);
		var dstOff = gl.bufferOffset;
		gl.bufferOffset += 4 * (dstCur - dstStart);

		gl.bufferSubData(gl.ARRAY_BUFFER, gl.bufferOffset, gbuf32, texStart, texCur - texStart);
		var texOff = gl.bufferOffset;
		gl.bufferOffset += 4 * (texCur - texStart);

		gl.bufferSubData(gl.ARRAY_BUFFER, gl.bufferOffset, gbuf8, colStart, colCur - colStart);
		var colOff = gl.bufferOffset;
		gl.bufferOffset += 1 * (colCur - colStart);

		gl.vertexAttribPointer(vertexPositionAttribute, 2, gl.FLOAT, false, 0, dstOff);
		gl.vertexAttribPointer(texcoordLocation, 2, gl.FLOAT, false, 0, texOff);
		gl.vertexAttribPointer(colorLocation1, 1, gl.UNSIGNED_BYTE, false, 0, colOff);

		draw.multi_draws++;
		gl.drawArrays(gl.TRIANGLES, 0, 6 * draws);
	},

	p_DrawImageQuad: function p_DrawImageQuad(id, x0, y0, x1, y1, x2, y2, x3, y3, s0, t0, s1, t1, s2, t2, s3, t3) {
		throw "Shouldn't be called directly";
	},
	p_SetDrawColor: function p_SetDrawColor(rgba) {
		draw.color = rgba;
	},
	p_SetViewport: function p_SetViewport(x, y, w, h) {
		if (w < 0 || h < 0)
			return;
		gl.viewport(x, glCanvas.height - y - h, w, h);
		matrix = m4.orthographic(0, w, h, 0, -1, 1);
		draw.viewport.x = x;
		draw.viewport.y = y;
		draw.viewport.w = w;
		draw.viewport.h = h;

		gl.uniformMatrix4fv(matrixLocation, false, matrix);

		draw.set_viewport++;
	},
	p_DrawString: function p_DrawString(x, y, align, size, font, text) {
		text = oversized(text);
		font = getFont(font);

		var textForMeasure = text.replace(/\^x.{6}/gi, "").replace(/\^./gi, "");
		var width = draw.DrawStringWidth(size, font, textForMeasure);

		var color = "#FFFFFF";
		if (text[0] == "^") {
			if (text[1] == "x") {
				color = "#" + text.substring(2, 8)
				text = text.substring(8);
			} else {
				color = COLOR_MAP[parseInt(text[1])];
				text = text.substring(2);
			}
		}
		var remainder = "";
		var pos = text.indexOf("^");
		if (pos != -1) {
			remainder = text.substring(pos);
			text = text.substring(0, pos);
		}

		var obj = cachedText.getTextureForString(size, font, text);
		switch (align) {
		case "CENTER":
			x += (draw.viewport.w - width) / 2;
			break;
		case "RIGHT":
			x = draw.viewport.w - width - x;
			break;
		case "CENTER_X":
			x -= width / 2;
			break;
		case "RIGHT_X":
			x -= width;
			break;
		}

		x = Math.round(x);
		y = Math.round(y);

		gl.bindTexture(gl.TEXTURE_2D, obj.tex);

		var w = obj.w;
		var h = obj.h;

		var vertices = [
			x, y,
			x+w, y,
			x+w, y+h,
			x, y+h
		];
		gl.vertexAttribPointer(vertexPositionAttribute, 2, gl.FLOAT, false, 0, writeBuffer(vertices));

		var vertices = [
			0, 0,
			obj.tw, 0,
			obj.tw, obj.th,
			0, obj.th
		];
		gl.vertexAttribPointer(texcoordLocation, 2, gl.FLOAT, false, 0, writeBuffer(vertices));

		color = color.substring(1);
		var r = parseInt(color.substring(0, 2), 16);
		var g = parseInt(color.substring(2, 4), 16);
		var b = parseInt(color.substring(4, 6), 16);
		var rgba = ((r) | (g << 8) | (b << 16) | (0xFF000000)) >>> 0;

		var idx = idx_by_color(rgba);
		var colors = [ idx, idx, idx, idx ];
		gl.vertexAttribPointer(colorLocation1, 1, gl.UNSIGNED_BYTE, false, 0, writeBufferByte(colors));

		draw.quad_calls++;
		gl.drawArrays(gl.TRIANGLE_FAN, 0, 4);

		if (remainder.length > 0)
			draw.p_DrawString(x + obj.w, y, "", size, font, remainder);
	},

	DrawStringWidth: function(size, font, text) {
		text = oversized(text);

		text = text.replace(/\^x.{6}/gi, "");
		text = text.replace(/\^./gi, "");

		var hash = size + ":" + font + ":" + text;
		if (cachedText.widthCache.hasOwnProperty(hash))
			return cachedText.widthCache[hash];

		font = getFont(font);
		cachedText.ctx.font = size + "px " + font;
		var lines = text.split("\n");
		var w = 0;
		for (var i = 0; i < lines.length; ++i)
			w = Math.max(w, cachedText.ctx.measureText(lines[i]).width);
		cachedText.widthCache[hash] = w;
		return w;
	},

	DrawStringCursorIndex: function(size, font, text, x, y) {
		text = oversized(text);

		if (y < 0)
			return 1;

		var lines = text.split("\n");

		var line_number = Math.min(lines.length, Math.ceil(y / (size + 2)));

		if (line_number == 0)
			return 1;

		var cnt = 0;
		for (var i = 0; i < line_number - 1; ++i)
			cnt += lines[i].length + 1; // +1 for \n

		var myline = lines[line_number - 1];
		var idx = 0;
		var cur = "";

		font = getFont(font);
		cachedText.ctx.font = size + "px " + font;

		while (true) {
			cur += myline[idx];

			idx += 1;
			cnt += 1;

			var fm = cachedText.ctx.measureText(cur);
			if (fm.width > x)
				break;
		}

		return cnt;
	},

	SetDrawSubLayer: function(sublayer) {
		draw.SetDrawLayer(draw.layer, sublayer);
	},
	SetDrawLayer: function(layer, sublayer) {
		draw.layer = layer;
		draw.sublayer = sublayer;
	}
};
