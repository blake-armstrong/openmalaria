"""Readers for openMalaria outputs and statistical reference files, the key aggregates, and
the per-cell rules shared by generate_reference.py and check_scenario.py.

Standard library only, so the single-run checker can use it without numpy.

A cell is one value of one run, keyed by (file, survey, group, measure), all strings:
- output.txt: ("survey", survey, group, measure), from the row's first three columns
- ctsout.txt: ("ctsout", <first column of the row>, "", <column header>)
- aggregates: ("aggregate", "", "", <aggregate name>)
"""

import math
from dataclasses import dataclass, field

SURVEY = "survey"
CTSOUT = "ctsout"
AGGREGATE = "aggregate"
FORMAT_VERSION = "2"
RUN_ALPHA = 0.01
LOO_COUNTS = "loo_miss_counts"
COLUMNS = ("file", "survey", "group", "measure", "mean", "sd", "min", "max", "lo", "hi")
STAT_COLUMNS = COLUMNS[4:]
WHOLE_POPULATION_GROUP_LIMIT = 1000

M_HOSTS = 0
M_PATENT = 3
M_UNCOMPLICATED = 14
M_SEVERE = 15
M_INDIRECT_DEATHS = 18
M_DIRECT_DEATHS = 19
M_SIMULATED_EIR = 36
CTS_SIMULATED_EIR = "simulated EIR"
AGGREGATE_SURVEY_MEASURES = {str(m) for m in (
    M_HOSTS, M_PATENT, M_UNCOMPLICATED, M_SEVERE, M_INDIRECT_DEATHS, M_DIRECT_DEATHS, M_SIMULATED_EIR)}


class FormatError(Exception):
    pass


def read_survey(path):
    keys = []
    values = []
    with open(path) as f:
        for number, line in enumerate(f, 1):
            items = line.split()
            if not items:
                continue
            if len(items) != 4:
                raise FormatError(f"{path}:{number}: expected 4 columns, found {len(items)}")
            survey, group, measure, value = items
            keys.append((SURVEY, str(int(survey)), str(int(group)), str(int(measure))))
            values.append(float(value))
    return keys, values


def read_ctsout(path):
    with open(path) as f:
        lines = [line.rstrip("\n") for line in f if line.strip()]
    if len(lines) < 2 or not lines[0].startswith("##"):
        raise FormatError(f"{path}: expected a '##' line and a header line")
    headers = unique_headers(lines[1].split("\t"))
    keys = []
    values = []
    for number, line in enumerate(lines[2:], 3):
        items = line.split("\t")
        if len(items) != len(headers):
            raise FormatError(f"{path}:{number}: expected {len(headers)} columns, found {len(items)}")
        for header, item in zip(headers[1:], items[1:], strict=True):
            keys.append((CTSOUT, items[0], "", header))
            values.append(float(item))
    return keys, values


def unique_headers(headers):
    seen = {}
    unique = []
    for header in headers:
        seen[header] = seen.get(header, 0) + 1
        unique.append(header if seen[header] == 1 else f"{header}#{seen[header]}")
    return unique


def read_run(directory):
    keys, values = read_survey(directory / "output.txt")
    ctsout = directory / "ctsout.txt"
    if ctsout.is_file():
        cts_keys, cts_values = read_ctsout(ctsout)
        keys += cts_keys
        values += cts_values
    return keys, values


def _whole_population_sum(cells, measure):
    found = False
    total = 0.0
    for (file, _, group, m), value in cells.items():
        if file == SURVEY and m == str(measure) and int(group) < WHOLE_POPULATION_GROUP_LIMIT:
            found = True
            total += value
    return total if found else None


def _sum_present(cells, measures):
    sums = [s for s in (_whole_population_sum(cells, m) for m in measures) if s is not None]
    return math.fsum(sums) if sums else None


def _mean_prevalence(cells):
    patent = _whole_population_sum(cells, M_PATENT)
    hosts = _whole_population_sum(cells, M_HOSTS)
    if patent is None or not hosts:
        return None
    return patent / hosts


def _mean(values):
    return math.fsum(values) / len(values) if values else None


def _mean_eir(cells):
    survey = [v for (file, _, g, m), v in cells.items()
              if file == SURVEY and m == str(M_SIMULATED_EIR) and int(g) < WHOLE_POPULATION_GROUP_LIMIT]
    if survey:
        return _mean(survey)
    return _mean([v for (file, _, _, m), v in cells.items()
                  if file == CTSOUT and m == CTS_SIMULATED_EIR and not math.isnan(v)])


AGGREGATES = {
    "clinical_episodes": lambda cells: _sum_present(cells, [M_UNCOMPLICATED]),
    "severe_episodes": lambda cells: _sum_present(cells, [M_SEVERE]),
    "deaths": lambda cells: _sum_present(cells, [M_INDIRECT_DEATHS, M_DIRECT_DEATHS]),
    "mean_prevalence": _mean_prevalence,
    "mean_eir": _mean_eir,
}


def is_aggregate_input(key):
    file, _, _, measure = key
    if file == SURVEY:
        return measure in AGGREGATE_SURVEY_MEASURES
    return file == CTSOUT and measure == CTS_SIMULATED_EIR


def aggregates(cells):
    return {name: value for name, f in AGGREGATES.items() if (value := f(cells)) is not None}


@dataclass
class Reference:
    meta: dict = field(default_factory=dict)
    rows: dict = field(default_factory=dict)
    loo_counts: list = field(default_factory=list)


def format_number(x):
    return repr(float(x))


def write_reference(path, meta, rows):
    with open(path, "w", newline="\n") as f:
        f.write("# OpenMalaria statistical reference\n")
        for key, value in meta.items():
            if "\n" in str(value) or ":" in key:
                raise ValueError(f"bad metadata entry {key!r}")
            f.write(f"# {key}: {value}\n")
        f.write(",".join(COLUMNS) + "\n")
        for key, stats in rows:
            if any("," in part or "\n" in part for part in key):
                raise ValueError(f"key {key!r} can't be written to CSV")
            f.write(",".join([*key, *(format_number(stats[c]) for c in STAT_COLUMNS)]) + "\n")


def read_reference(path):
    ref = Reference()
    header_seen = False
    with open(path) as f:
        for number, line in enumerate(f, 1):
            line = line.rstrip("\n")
            if line.startswith("#"):
                key, sep, value = line[1:].partition(":")
                if sep:
                    ref.meta[key.strip()] = value.strip()
                continue
            parts = line.split(",")
            if not header_seen:
                if tuple(parts) != COLUMNS:
                    raise FormatError(f"{path}:{number}: expected header {','.join(COLUMNS)}")
                header_seen = True
                continue
            if len(parts) != len(COLUMNS):
                raise FormatError(f"{path}:{number}: expected {len(COLUMNS)} fields, found {len(parts)}")
            ref.rows[tuple(parts[:4])] = dict(zip(STAT_COLUMNS, map(float, parts[4:]), strict=True))
    if ref.meta.get("format_version") != FORMAT_VERSION:
        raise FormatError(f"{path}: format_version {ref.meta.get('format_version')}, expected {FORMAT_VERSION}")
    try:
        ref.loo_counts = [int(c) for c in ref.meta[LOO_COUNTS].split()]
    except (KeyError, ValueError) as e:
        raise FormatError(f"{path}: bad or missing {LOO_COUNTS}") from e
    if str(len(ref.loo_counts)) != ref.meta.get("replicates"):
        raise FormatError(f"{path}: {len(ref.loo_counts)} {LOO_COUNTS} for {ref.meta.get('replicates')} replicates")
    return ref


def run_p_value(misses, loo_counts):
    return (1 + sum(1 for c in loo_counts if c >= misses)) / (len(loo_counts) + 1)


def is_fixed_cell(row):
    return math.isnan(row["min"]) or (row["min"] == row["max"] and row["min"] != 0)


def same_value(a, b):
    return a == b or (math.isnan(a) and math.isnan(b))


def is_miss(value, row):
    if math.isnan(row["lo"]) or math.isnan(value):
        return not same_value(value, row["lo"])
    return value < row["lo"] or value > row["hi"]
