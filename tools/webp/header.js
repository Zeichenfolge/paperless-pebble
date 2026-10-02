/*
 * WebP decoder for PebbleKit JS (pure JavaScript, no canvas/WebAssembly needed).
 * Built from image-in-browser 3.6.0 (MIT License, Copyright (c) 2022 Yegor Pelykh,
 * https://github.com/image-js/image-in-browser). Its WebP code is derived from libwebp
 * (BSD-3-Clause, Copyright (c) 2010 Google Inc.). Full license texts: THIRD_PARTY_NOTICES.md
 * Bundled with esbuild and transpiled to ES5 with Babel so it runs in the Pebble SDK's
 * webpack 1 and in the iOS app's JavaScriptCore. Rebuild: tools/webp (npm run build).
 * Exports: decodeWebPGray(Uint8Array) -> { width, height, gray: Uint8Array }
 */
(function (g) {
  // iOS PebbleKit JS has no TextDecoder; the library constructs one at load time
  // (only used for EXIF strings). Minimal Latin-1 fallback.
  if (typeof g.TextDecoder === 'undefined') {
    g.TextDecoder = function () {};
    g.TextDecoder.prototype.decode = function (b) {
      if (!b) return '';
      var u = b instanceof Uint8Array ? b : new Uint8Array(b.buffer || b);
      var s = '';
      for (var i = 0; i < u.length; i++) s += String.fromCharCode(u[i]);
      return s;
    };
  }
})(typeof globalThis !== 'undefined' ? globalThis : (typeof self !== 'undefined' ? self : this));
