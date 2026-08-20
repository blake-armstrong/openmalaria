export interface RunOptions {
    /** Validate the scenario (parse, schema-check, build the model graph)
     * without running the timestep loop. Much cheaper than a full run;
     * `RunResult.output` is omitted since no output.txt is written. */
    validateOnly?: boolean;
    /** Called once per line of stdout produced during the run. */
    onStdout?: (line: string) => void;
    /** Called once per line of stderr (progress/warnings, not necessarily
     * errors -- see RunResult.warnings for the same lines collected). */
    onStderr?: (line: string) => void;
}

export interface RunResult {
    /** Contents of output.txt: tab-separated survey data, no header, one
     * row per (survey, group, measureId, value). Omitted when
     * `validateOnly` was set. See API.md for the format and measure IDs. */
    output?: string;
    /** Contents of ctsout.txt (continuous/time-series output), present only
     * if the scenario's <monitoring> declares <continuous>. */
    cts?: string;
    /** All stderr lines emitted during the run (progress + warnings). */
    warnings: string[];
}

/** Thrown when the wasm module's main() returns a non-zero exit code. */
export class OpenMalariaError extends Error {
    constructor(message: string, code: number, stderr: string[]);
    /** OpenMalaria's process exit code -- see model/util/errors.h for the
     * full Error::ErrorCodes enum (e.g. 67 = XSD/schema error). */
    code: number;
    /** All stderr lines emitted before the failure. */
    stderr: string[];
}

/**
 * Run an OpenMalaria scenario to completion in a fresh, isolated wasm
 * module instance.
 *
 * Throws a synchronous TypeError if `scenarioXml` is not a non-empty
 * string (a caller error). Returns a Promise that rejects with an
 * OpenMalariaError if the simulation itself fails.
 */
export function runScenario(scenarioXml: string, options?: RunOptions): Promise<RunResult>;

export default runScenario;
