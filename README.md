# OpenMalaria

This git-repository contains source-code for OpenMalaria, a simulator program
for studying malaria epidemiology and the impacts of interventions against
malaria.

______________________________________________________________________

For further documentation, take a look at our
[wiki](https://github.com/OpenMalaria-Org/openmalaria/wiki).

Also find our code on zenodo, with DOI-references:
[![DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.10534022.svg)](https://doi.org/10.5281/zenodo.10534022)

Schema documentation for the XML input files can be found
[on this website](https://swisstph.github.io/openmalaria/) or in the `schema`
folder (both past releases and development version).

You can download the latest build here:
[releases](https://github.com/OpenMalaria-Org/openmalaria/releases)

# Version

The schema version is specified in the following places (all need to be updated
when releasing a new version):

- DocumentLoader.h
- schema/scenario.xsd, demography.xsd, etc. (all XSD files without a version
  number)
- schema/CMakeLists.txt (namespace-map)
- copy build/schema/scenario_current.xsd to schema/scenario_XX.xsd
- test/\*.xml — update http://openmalaria.org/schema/scenario_XX and
  (optionally) schemaVersion="XX"
- version.txt — needed for build service

In theory the "schema namespace version" doesn't need to match the "OpenMalaria"
version and we could update the latter without requiring changes to XML files,
however currently we keep both synchronised (in some ways this is simpler).

The `emscripten` build also hardcodes the schema version:

- emscripten/build-model.sh — the
  `cp schema/scenario_XX.xsd .../scenario_current.xsd` line that stages the
  schema embedded into the wasm module

The `js` package also references the schema version, in the following places:

- js/API.md — "currently schema version XX" (twice)
- js/test/fixtures/scenario1.xml — the
  `http://openmalaria.org/schema/scenario_XX` namespace and `schemaVersion="XX"`
- js/package.json — the `description` field's "(targets schema version XX)"

The `python` package's example scenario also references the schema version:

- python/examples/scenario1.xml — the
  `http://openmalaria.org/schema/scenario_XX` namespace and `schemaVersion="XX"`
- python/examples/scenario_XX.xsd
- python/pyproject.toml — the `description` field's "(targets schema version
  XX)"

## Binding package versions (Python / JS)

The `openmalaria` PyPI package and `openmalaria` npm package versions are
separate from the schema version (and from each other).
They are not synchronised with each other. Their versions are defined in
`bindings_version.txt` at the repo root; run `scripts/sync_bindings_version.sh`
to propagate it into `python/pyproject.toml` and `js/package.json`, and to get a
warning if either package's "(targets schema version XX)" description looks out
of date relative to `version.txt`.

# Installation instructions:

See
[INSTALLATION](https://github.com/OpenMalaria-Org/openmalaria/wiki/UserGuide).

# Build instructions:

```
mkdir build && cd build
ccmake ..
Press 'c', look over options, press 'c' again and 'g'
make -j4
ctest -j4
```

For testing and development, ideally use debug builds (which enable some asserts
to do with simulation time usage).

**JavaScript / WebAssembly**: the `openmalaria` npm package runs the OpenMalaria
model compiled to WebAssembly, in Node or a browser with zero server dependency;
see [js/README.md](js/README.md#building-from-source) for build/install
instructions.

**Python**: the `openmalaria` PyPI package provides nanobind-based Python
bindings that run a scenario in-process and return pandas DataFrames directly;
see [python/README.md](python/README.md#install) for installation instructions.

# Code subdirectories:

|- dir -|- description -|
|----------|:-------------------------------------------------------------------------:|
| contrib                                           | Third-party libraries, distributed under the same repo for convenience. |
| model                                             | Source code for the malaria model. |
| test                                              | High-level testing: test scenarios with expected outputs. Also run-time files: densities.csv, scenario\_?.xsd, Nv0scenario\*.txt. |
| unittest                                          | Low-level testing: unittests for the model using cxxunit. |
| util                                              | Extra scripts associated with OpenMalaria. |
| schema                                            | scenario schema files (see schema/policy.txt for details) |
| schema/scenario.xsd                               | The latest (partial) schema file. |
| schema/entomology.xsd, schema/demography.xsd, etc | components of the latest schema, included from scenario.xsd. |
| schema/scenario\_\*.xsd                           | Copies of released schema versions, with all components inlined in the same file. |
| emscripten                                        | Build toolchain that cross-compiles the model (and its GSL/Xerces-C dependencies) to a WebAssembly module for the js package. |
| js                                                | `openmalaria` npm package: JavaScript bindings that run the wasm-compiled model in Node or a browser. |
| python                                            | `openmalaria` PyPI package: nanobind bindings that run the model in-process and return pandas DataFrames. |

This git repository is currently maintained by members of the
[Global Disease Modelling Team](https://www.thekids.org.au/our-research/early-environment/infection-and-vaccines/global-disease-modelling/)
of __The Kids Research Institute Australia__, the
[Disease Modelling Group](https://www.swisstph.ch/en/about/eph/disease-modelling/)
of the __Swiss Tropical and Public Health institute__ along with the
[SciCORE Center for Scientific Computing, University of Basel](https://scicore.unibas.ch)
and other collaborators.

# License

OpenMalaria is distributed under the terms of the
[GPL v2](http://opensource.org/licenses/GPL-2.0) (also see [COPYING](COPYING)),
or, (at your option) any later version.
