import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { runScenario, OpenMalariaError } from '../src/index.mjs';

const scenarioXml = readFileSync(
    fileURLToPath(new URL('./fixtures/scenario1.xml', import.meta.url)),
    'utf8'
);

test('throws a synchronous TypeError on bad input', () => {
    assert.throws(() => runScenario(''), TypeError);
    assert.throws(() => runScenario(null), TypeError);
    assert.throws(() => runScenario(42), TypeError);
});

test('runs a scenario and returns TSV output', async () => {
    const result = await runScenario(scenarioXml);
    assert.equal(typeof result.output, 'string');
    assert.ok(result.output.length > 0);
    // No header row: every line is tab-separated survey/group/measure/value.
    const firstLine = result.output.split('\n')[0];
    assert.equal(firstLine.split('\t').length, 4);
});

test('two runs of the same scenario produce identical output (deterministic, isolated instances)', async () => {
    const [a, b] = await Promise.all([runScenario(scenarioXml), runScenario(scenarioXml)]);
    assert.equal(a.output, b.output);
});

test('validateOnly skips the simulation and omits output', async () => {
    const result = await runScenario(scenarioXml, { validateOnly: true });
    assert.equal(result.output, undefined);
});

test('validateOnly still rejects a schema-invalid scenario', async () => {
    const badXml = scenarioXml.replace('popSize="100"', 'popSize="not-a-number"');
    await assert.rejects(
        () => runScenario(badXml, { validateOnly: true }),
        (err) => {
            assert.ok(err instanceof OpenMalariaError);
            assert.equal(err.code, 67); // Error::XSD, model/util/errors.h
            return true;
        }
    );
});

test('a malformed scenario rejects with OpenMalariaError, not a full run', async () => {
    const badXml = scenarioXml.replace('popSize="100"', 'popSize="not-a-number"');
    await assert.rejects(() => runScenario(badXml), OpenMalariaError);
});
