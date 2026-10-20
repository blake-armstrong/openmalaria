"""Readers for openMalaria outputs and statistical reference files, the key aggregates, and
the per-cell rules shared by generate_reference.py and check_scenario.py.

Standard library only, so the single-run checker can use it without numpy.

A cell is one value of one run, keyed by (file, survey, group, measure), all strings:
- output.txt: ("survey", survey, group, measure), from the row's first three columns
- ctsout.txt: ("ctsout", <first column of the row>, "", <column header>). A ctsout written during
  initialisation (`<continuous duringInit="true">`) starts with a "simulation time" column and has
  timestep nan on warmup rows, whose number depends on the vector warmup's rounds. Its rows are
  keyed by the timestep instead, and the warmup rows and the "simulation time" column are left out.
- aggregates: ("aggregate", "", "", <aggregate name>)

A survey measure or ctsout column with a cell that is NaN in some replicates but not all (such as
simulatedEIR, the mean EIR of adults, in a survey of a tiny population that has no adults) has no
per-cell statistics: the reference lists it under nan_measures, and its cells are left out of a
run's per-cell checks and measure totals. The aggregates still use them, skipping NaN values.

The group test of check_scenario.py --replicates compares the mean of K runs for each group
quantity: every aggregate, and every survey measure's total over surveys and groups, keyed
("total", "", "whole" or "split", measure), with whole-population groups (below 1000) and
cohort or genotype groups (1000 and above) totalled separately.
"""

import gzip
import math
import statistics
from dataclasses import dataclass, field
from functools import cached_property

SURVEY = "survey"
CTSOUT = "ctsout"
SIMULATION_TIME = "simulation time"
TIMESTEP = "timestep"
AGGREGATE = "aggregate"
TOTAL = "total"
WHOLE = "whole"
SPLIT = "split"
FORMAT_VERSION = "4"
NAN_MEASURES = "nan_measures"
MEASURE_SEPARATOR = " | "
RUN_ALPHA = 0.01
LOO_COUNTS = "loo_miss_counts"
REFERENCE_SUFFIX = ".csv.gz"
COLUMNS = ("file", "survey", "group", "measure", "mean", "sd", "min", "max", "lo", "hi")
STAT_COLUMNS = COLUMNS[4:]
GROUP_COLUMNS = (*COLUMNS[:4], "values")
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
    key_column = headers.index(TIMESTEP) if headers[0] == SIMULATION_TIME and TIMESTEP in headers else 0
    keys = []
    values = []
    for number, line in enumerate(lines[2:], 3):
        items = line.split("\t")
        if len(items) != len(headers):
            raise FormatError(f"{path}:{number}: expected {len(headers)} columns, found {len(items)}")
        key = items[key_column]
        if key_column and math.isnan(float(key)):
            continue
        for column, (header, item) in enumerate(zip(headers, items, strict=True)):
            if column not in (0, key_column):
                keys.append((CTSOUT, key, "", header))
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
        return _mean([v for v in survey if not math.isnan(v)])
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


def total_key(group, measure):
    return TOTAL, "", WHOLE if int(group) < WHOLE_POPULATION_GROUP_LIMIT else SPLIT, measure


def measure_totals(cells):
    values = {}
    for (file, _, group, measure), value in cells.items():
        if file == SURVEY:
            values.setdefault(total_key(group, measure), []).append(value)
    return {key: math.fsum(v) for key, v in values.items()}


def group_quantities(cells, excluded=frozenset()):
    quantities = {(AGGREGATE, "", "", name): value for name, value in aggregates(cells).items()}
    quantities.update(measure_totals(without_measures(cells, excluded)))
    return quantities


def format_measures(measures):
    return MEASURE_SEPARATOR.join(f"{file}:{measure}" for file, measure in sorted(measures)) or "none"


def parse_measures(text):
    if text in ("", "none"):
        return frozenset()
    return frozenset(tuple(item.split(":", 1)) for item in text.split(MEASURE_SEPARATOR))


def without_measures(cells, measures):
    if not measures:
        return cells
    return {key: value for key, value in cells.items() if fixed_cell_group(key) not in measures}


@dataclass
class Reference:
    meta: dict = field(default_factory=dict)
    rows: dict = field(default_factory=dict)
    loo_counts: list = field(default_factory=list)
    group_values: dict = field(default_factory=dict)

    @cached_property
    def fixed(self):
        return fixed_keys(self.rows)

    @cached_property
    def nan_measures(self):
        return parse_measures(self.meta.get(NAN_MEASURES, ""))

    @cached_property
    def group_stats(self):
        return {key: (statistics.fmean(v), statistics.stdev(v)) for key, v in self.group_values.items()}


def format_number(x):
    return repr(float(x))


def format_value(x):
    text = repr(float(x))
    return text.removesuffix(".0")


def check_key(key):
    if any("," in part or "\n" in part for part in key):
        raise ValueError(f"key {key!r} can't be written to CSV")


def is_gzip(path):
    return str(path).endswith(".gz")


def write_reference(path, meta, rows, group_values=None):
    lines = ["# OpenMalaria statistical reference\n"]
    for key, value in meta.items():
        if "\n" in str(value) or ":" in key:
            raise ValueError(f"bad metadata entry {key!r}")
        lines.append(f"# {key}: {value}\n")
    lines.append(",".join(COLUMNS) + "\n")
    for key, stats in rows:
        check_key(key)
        lines.append(",".join([*key, *(format_number(stats[c]) for c in STAT_COLUMNS)]) + "\n")
    if group_values:
        lines.append(",".join(GROUP_COLUMNS) + "\n")
        for key, values in group_values.items():
            check_key(key)
            lines.append(",".join([*key, " ".join(map(format_value, values))]) + "\n")
    data = "".join(lines).encode()
    with open(path, "wb") as f:
        f.write(gzip.compress(data, compresslevel=9, mtime=0) if is_gzip(path) else data)


def read_reference(path):
    ref = Reference()
    header_seen = False
    in_group_values = False
    with gzip.open(path, "rt") if is_gzip(path) else open(path) as f:
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
            if tuple(parts) == GROUP_COLUMNS:
                in_group_values = True
                continue
            if in_group_values:
                if len(parts) != len(GROUP_COLUMNS):
                    raise FormatError(f"{path}:{number}: expected {len(GROUP_COLUMNS)} fields, found {len(parts)}")
                ref.group_values[tuple(parts[:4])] = [float(v) for v in parts[4].split()]
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
    for key, values in ref.group_values.items():
        if len(values) != len(ref.loo_counts):
            raise FormatError(f"{path}: {len(values)} values of {key} for {len(ref.loo_counts)} replicates")
    return ref


def run_p_value(misses, loo_counts):
    return (1 + sum(1 for c in loo_counts if c >= misses)) / (len(loo_counts) + 1)


def fixed_cell_group(key):
    file, _, _, measure = key
    return file, measure


def is_constant(row):
    return row["min"] == row["max"] or math.isnan(row["min"])


def is_cell(key):
    return key[0] in (SURVEY, CTSOUT)


def fixed_keys(rows):
    constant = {}
    for key, row in rows.items():
        if is_cell(key):
            group = fixed_cell_group(key)
            constant[group] = constant.get(group, True) and is_constant(row)
    return {key for key, row in rows.items()
            if is_cell(key) and constant[fixed_cell_group(key)] and row["min"] != 0}


def same_value(a, b):
    return a == b or (math.isnan(a) and math.isnan(b))


def is_miss(value, row):
    if math.isnan(row["lo"]) or math.isnan(value):
        return not same_value(value, row["lo"])
    return value < row["lo"] or value > row["hi"]
