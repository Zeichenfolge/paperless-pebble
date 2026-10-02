// Bildaufbereitung für die Uhr: skalieren, auf 4 Graustufen rastern, 2 Bit packen.
// Reines ES5, damit es im Pebble-SDK (webpack 1) und in iOS JavaScriptCore läuft.

var SPLIT_VERTICAL = 0;
var SPLIT_HORIZONTAL = 1;
var SPLIT_NONE = 2;
var MAX_ZOOM_EDGE = 1000;   // längste Kante des Zoom-Bildes (Pixel)

// Ausschnitt (cx, cy, cw, ch) aus einem Graustufenbild auf dw x dh skalieren.
// Box-Filter: jeder Zielpixel ist der Mittelwert der überdeckten Quellpixel.
function scaleGray(src, sw, sh, cx, cy, cw, ch, dw, dh) {
  var out = new Uint8Array(dw * dh);
  var y, x, yy, xx;
  for (y = 0; y < dh; y++) {
    var y0 = Math.floor(cy + (y * ch) / dh);
    var y1 = Math.max(y0 + 1, Math.ceil(cy + ((y + 1) * ch) / dh));
    if (y0 >= sh) y0 = sh - 1;
    if (y1 > sh) y1 = sh;
    for (x = 0; x < dw; x++) {
      var x0 = Math.floor(cx + (x * cw) / dw);
      var x1 = Math.max(x0 + 1, Math.ceil(cx + ((x + 1) * cw) / dw));
      if (x0 >= sw) x0 = sw - 1;
      if (x1 > sw) x1 = sw;
      var sum = 0, n = 0;
      for (yy = y0; yy < y1; yy++) {
        var row = yy * sw;
        for (xx = x0; xx < x1; xx++) {
          sum += src[row + xx];
          n++;
        }
      }
      out[y * dw + x] = n ? Math.round(sum / n) : 255;
    }
  }
  return out;
}

// Kontrast leicht anheben: Papier wird reinweiß, dünne Schrift dunkler.
function enhance(gray) {
  var lut = new Uint8Array(256);
  for (var v = 0; v < 256; v++) {
    var t = (v - 30) / (235 - 30);
    t = t < 0 ? 0 : (t > 1 ? 1 : t);
    lut[v] = Math.round(Math.pow(t, 1.4) * 255);
  }
  for (var i = 0; i < gray.length; i++) gray[i] = lut[gray[i]];
  return gray;
}

// Floyd-Steinberg auf 4 Stufen (0 = schwarz ... 3 = weiß), gepackt mit 2 Bit pro Pixel.
// Erstes Pixel einer Zeile in den obersten Bits; jede Zeile beginnt auf einem neuen Byte.
function ditherPack2(gray, w, h) {
  var rowBytes = Math.ceil(w / 4);
  var out = new Uint8Array(rowBytes * h);
  var cur = new Float32Array(w + 2);
  var next = new Float32Array(w + 2);
  var x, y;
  for (x = 0; x < w; x++) cur[x + 1] = gray[x];
  for (y = 0; y < h; y++) {
    for (x = 0; x < w + 2; x++) next[x] = 0;
    if (y + 1 < h) {
      for (x = 0; x < w; x++) next[x + 1] = gray[(y + 1) * w + x];
    }
    for (x = 0; x < w; x++) {
      var v = cur[x + 1];
      var level = Math.round(v / 85);
      if (level < 0) level = 0;
      if (level > 3) level = 3;
      var err = v - level * 85;
      cur[x + 2] += err * 7 / 16;
      next[x] += err * 3 / 16;
      next[x + 1] += err * 5 / 16;
      next[x + 2] += err / 16;
      out[y * rowBytes + (x >> 2)] |= level << (6 - 2 * (x & 3));
    }
    var t = cur; cur = next; next = t;
  }
  return { data: out, rowBytes: rowBytes };
}

// Aufteilung einer Länge in Streifen der Größe `view` mit etwas Überlappung
function parts(total, view) {
  if (total <= view) return { n: 1, step: 0 };
  var n = Math.ceil(total / view);
  if (n * view - total < view / 3) n++;
  return { n: n, step: Math.floor((total - view) / (n - 1)) };
}

// Bytes, die der Zoom-Ausschnitt auf der Uhr braucht
function zoomBytes(zw, zh, split, sw, sh) {
  if (split === SPLIT_VERTICAL) return Math.ceil(Math.min(sw, zw) / 4) * zh;
  if (split === SPLIT_HORIZONTAL) return Math.ceil(zw / 4) * Math.min(sh, zh);
  return Math.ceil(zw / 4) * zh;
}

// Größe des Zoom-Bildes: möglichst Originalauflösung, aber passend zum freien Speicher der Uhr
function geometry(w, h, split, sw, sh, maxBytes) {
  var s = Math.min(1, MAX_ZOOM_EDGE / Math.max(w, h));
  var zw, zh;
  for (var i = 0; i < 60; i++) {
    zw = Math.max(1, Math.floor(w * s));
    zh = Math.max(1, Math.floor(h * s));
    if (zoomBytes(zw, zh, split, sw, sh) <= maxBytes) break;
    s *= 0.95;
  }
  var p = { n: 1, step: 0 };
  if (split === SPLIT_VERTICAL) p = parts(zw, sw);
  else if (split === SPLIT_HORIZONTAL) p = parts(zh, sh);
  return { scale: s, zw: zw, zh: zh, n: p.n, step: p.step };
}

// Berechnet Bild und Aufteilung für die Uhr.
//  o.mode 0: ganze Seite, passend in o.screenW x o.screenH
//  o.mode 1: Zoom – Streifen o.part (vertikal/horizontal) oder ganze Seite (nicht teilen)
function render(img, o) {
  var w = img.width, h = img.height;
  var split = o.split === SPLIT_HORIZONTAL || o.split === SPLIT_NONE ? o.split : SPLIT_VERTICAL;
  var sw = o.screenW, sh = o.screenH;
  var g = geometry(w, h, split, sw, sh, o.maxBytes || 30000);
  var part = Math.max(0, Math.min(o.part || 0, g.n - 1));
  var s = g.scale;

  var gray, dw, dh;
  if (o.mode === 1) {
    if (split === SPLIT_VERTICAL) {
      dw = Math.min(sw, g.zw);
      dh = g.zh;
      gray = scaleGray(img.gray, w, h, (part * g.step) / s, 0, dw / s, h, dw, dh);
    } else if (split === SPLIT_HORIZONTAL) {
      dw = g.zw;
      dh = Math.min(sh, g.zh);
      gray = scaleGray(img.gray, w, h, 0, (part * g.step) / s, w, dh / s, dw, dh);
    } else {
      dw = g.zw;
      dh = g.zh;
      gray = scaleGray(img.gray, w, h, 0, 0, w, h, dw, dh);
    }
  } else {
    var f = Math.min(sw / w, sh / h);
    dw = Math.max(1, Math.round(w * f));
    dh = Math.max(1, Math.round(h * f));
    gray = scaleGray(img.gray, w, h, 0, 0, w, h, dw, dh);
  }
  enhance(gray);
  var packed = ditherPack2(gray, dw, dh);
  return {
    width: dw,
    height: dh,
    data: packed.data,
    parts: g.n,
    step: g.step,
    srcW: g.zw,
    srcH: g.zh,
    part: part,
    split: split
  };
}

module.exports = {
  scaleGray: scaleGray,
  enhance: enhance,
  ditherPack2: ditherPack2,
  geometry: geometry,
  render: render
};
