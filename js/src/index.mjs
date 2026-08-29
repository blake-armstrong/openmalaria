import createOpenMalaria from '../wasm/openMalaria.mjs';

const WORK_DIR = '/work';
const SCENARIO_FILE = 'scenario.xml';
const OUTPUT_FILE = 'output.txt';
const CTS_FILE = 'ctsout.txt';

/**
 * Thrown when the wasm module's main() returns a non-zero exit code (a
 * scenario/simulation failure), as distinct from a synchronous TypeError
 * thrown when the caller passes bad arguments.
 */
export class OpenMalariaError extends Error {
    constructor(message, code, stderr) {
        super(message);
        this.name = 'OpenMalariaError';
        this.code = code;
        this.stderr = stderr;
    }
}

/**
 * Run an OpenMalaria scenario to completion.
 *
 * Instantiates a fresh wasm module per call: OpenMalaria's C++ uses global
 * static state (sim, master_RNG, mon::internal::runtime, ModelOptions,
 * InterventionManager, Genotypes, CommandLine's own static fields) that's
 * set up once per run and never reset, so two runs must never share a
 * module instance. This is also what makes concurrent calls safe.
 *
 * @param {string} scenarioXml - The scenario XML document contents.
 * @param {object} [options]
 * @param {boolean} [options.validateOnly] - Parse, schema-validate, and
 *   build the model graph, then return without running the timestep loop
 *   (maps to the CLI's --validate-only). Much cheaper than a full run;
 *   `result.output` is omitted since no output.txt is written. Still
 *   surfaces structural/parameter errors via a rejected OpenMalariaError.
 * @param {(line: string) => void} [options.onStdout] - Called per stdout line.
 * @param {(line: string) => void} [options.onStderr] - Called per stderr line
 *   (progress/warnings, not necessarily errors -- see result.warnings).
 * @returns {Promise<{output?: string, cts?: string, warnings: string[]}>}
 * @throws {TypeError} synchronously if scenarioXml is not a non-empty string,
 * which is a caller error, distinct from a simulation failure.
 */
export function runScenario(scenarioXml, options = {}) {
    if (typeof scenarioXml !== 'string' || scenarioXml.length === 0) {
        throw new TypeError('runScenario: scenarioXml must be a non-empty string');
    }
    return runScenarioAsync(scenarioXml, options);
}

async function runScenarioAsync(scenarioXml, options) {
    const warnings = [];

    const Module = await createOpenMalaria({
        print: (line) => options.onStdout?.(line),
        printErr: (line) => {
            warnings.push(line);
            options.onStderr?.(line);
        },
    });

    const FS = Module.FS;
    FS.chdir(WORK_DIR);
    FS.writeFile(SCENARIO_FILE, scenarioXml);

    const argv = ['-s', SCENARIO_FILE, '-o', OUTPUT_FILE];
    if (options.validateOnly) argv.push('--validate-only');

    // Emscripten's Node target sets the process-global `process.exitCode` as
    // a side effect of every callMain() call, success or failure alike.
    // It's modelling a CLI process exit, which is meaningless here since
    // this library communicates success/failure through the returned
    // Promise instead. Restore whatever the host process had before this
    // call so that running multiple scenarios in one Node process (or this
    // package's own test suite) doesn't leave some arbitrary prior run's
    // exit code stuck on the whole process.
    const savedExitCode = typeof process !== 'undefined' ? process.exitCode : undefined;
    const code = Module.callMain(argv);
    if (typeof process !== 'undefined') process.exitCode = savedExitCode;

    if (code !== 0) {
        throw new OpenMalariaError(`OpenMalaria exited with code ${code}`, code, warnings);
    }

    let output;
    try {
        output = FS.readFile(OUTPUT_FILE, { encoding: 'utf8' });
    } catch {
        // Not written in --validate-only mode.
    }

    let cts;
    try {
        cts = FS.readFile(CTS_FILE, { encoding: 'utf8' });
    } catch {
        // Only written if the scenario's <monitoring> declares <continuous>.
    }

    return { output, cts, warnings };
}

export default runScenario;
