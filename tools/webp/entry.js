import { WebPDecoder } from 'image-in-browser/lib/src/formats/webp-decoder.js';
// Decodes WebP bytes to an 8-bit grayscale buffer (luma).
export function decodeWebPGray(bytes) {
  var img = new WebPDecoder().decode({ bytes: bytes });
  if (!img) throw new Error('WebP decode failed');
  var w = img.width, h = img.height, nc = img.numChannels;
  var src = img.toUint8Array();
  var out = new Uint8Array(w * h);
  for (var i = 0, j = 0; i < out.length; i++, j += nc) {
    out[i] = nc >= 3 ? ((src[j] * 77 + src[j + 1] * 150 + src[j + 2] * 29) >> 8) : src[j];
  }
  return { width: w, height: h, gray: out };
}
