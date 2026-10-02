#!/bin/sh
# Rebuilds ../../src/pkjs/webp.js
#   cd tools/webp && npm install && npm run build
set -e
npx esbuild entry.js --bundle --format=cjs --target=es2015 --outfile=out.es2015.js --log-level=warning
npx babel out.es2015.js -o out.es5.js
npx esbuild out.es5.js --minify --target=es5 --outfile=out.min.js --log-level=warning
cat header.js out.min.js > ../../src/pkjs/webp.js
rm -f out.es2015.js out.es5.js out.min.js
echo "src/pkjs/webp.js updated"
