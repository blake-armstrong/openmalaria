# openmalaria-wasm

The [OpenMalaria](https://github.com/SwissTPH/openmalaria) malaria simulation
model, compiled to WebAssembly, runnable in Node or a browser with zero server
dependency.

```js
import { runScenario } from 'openmalaria-wasm';
import { readFileSync } from 'node:fs';

const scenarioXml = readFileSync('scenario.xml', 'utf8');
const { output } = await runScenario(scenarioXml);
console.log(output);
```

See [API.md](./API.md) for the full API reference, output format, error
contract, and browser integration notes (Web Worker requirement, no COOP/COEP
needed, bundler asset handling).

## Building from source

The wasm module isn't built by this package's own scripts, it's produced by
`../emscripten/build-model.sh` in the parent repo (which in turn depends on
`../emscripten/deps/build-gsl.sh` and `build-xerces.sh`), and copied into
`wasm/` here. See `../emscripten/README.md` for the full build design and
rationale.

## License

GPL-3.0-or-later. OpenMalaria itself is GPL-2.0-or-later, but this package
statically links GSL (GPL-3.0-or-later), and the combined binary inherits the
stricter license.
