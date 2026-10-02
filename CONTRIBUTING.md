# Contributing

Thanks for helping! Bug reports and ideas are just as welcome as code.
German or English – both are fine.

## Reporting bugs and ideas

Use the [issue templates](../../issues/new/choose). For bugs, please include your
watch model, phone (iOS/Android), app version and Paperless-ngx version.
Logs help a lot: connect the watch (Dev Connect) and run `pebble logs --cloudpebble`.

**Never post your API token or your server URL** in an issue.

## Development setup

```bash
uv tool install pebble-tool --python 3.13
pebble sdk install latest
pebble build
pebble install --emulator emery
pebble emu-app-config        # opens the settings page for the emulator
```

## Things to know

- **Protocol:** message types, commands and fields are defined twice –
  `src/c/paperless.h` (watch) and the `MSG`/`CMD` tables in `src/pkjs/index.js` (phone).
  New AppMessage keys also go into `messageKeys` in `package.json`.
- **Phone code is ES5.** The Pebble SDK bundles PebbleKit JS with webpack 1, and the
  iOS app runs it in plain JavaScriptCore (no DOM, no canvas, no `TextDecoder`).
  Please avoid `let`/`const`, arrow functions, classes and `async`.
- **`src/pkjs/webp.js` is generated.** Rebuild it with `cd tools/webp && npm install && npm run build`.
- **Watch memory:** Pebble Time 2 has ~105 KB heap. The image view asks the phone for
  images that fit the free memory (`MaxBytes`).
- **Adding a language:** `src/c/i18n.c` (watch), `TEXT` in `src/pkjs/index.js` (messages),
  `TEXT` in `src/pkjs/config.js` (settings page), and the language list in `config.js`.

## Pull requests

- One topic per pull request
- `pebble build` must pass without warnings (CI checks this)
- Test on the emulator (`emery`, ideally also `basalt`) and describe what you tested
- Screenshots for UI changes are great – please use fictional documents, not your own
