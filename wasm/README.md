# Crossplay WebAssembly build

Run the build from the repository root:

```sh
./wasm/build.sh
```

The Emscripten Python runtime must be Python 3.10 or newer.

The build writes `quackle.js`, `quackle.wasm`, and `quackle.data` to
`wasm/dist/`. The directory is not tracked.

The tracked NWL23 DAWG is enough for a functional build. For faster move
generation, place `nwl23.gaddag` in `data/lexica/` before the build. The
analyzer artifact used this optional file:

```text
SHA-256 07d12c5142110f47564196f917b4324a995c0c6782d89ddc6793272d6b828cd4
```

Do not add dictionary source text or generated lexicon files to a commit
without a separate license and repository-size review.
