#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# dependencies = ["lxml>=5"]
# ///
"""Write copies of the referenced scenarios with one intervention switched off, for S6's power study.

A human component's deployments are removed (only that component, where it shares a deployment
with others); for the other kinds the element is removed. Usage: make_intervention_off.py OUT_DIR
"""

import sys
from pathlib import Path

from lxml import etree

ROOT = Path(__file__).resolve().parents[2]
HUMAN = {
    "12": "MSAT", "2ITNs": "ITN", "5": "PEV", "6": "PEV", "9": "clearInfections", "BSV": "Vaccine",
    "Cohort": "MDA", "DecisionTree5DayDielmo": "MSAT", "GaussianCopulaGamma": "kill_repell",
    "GaussianCopulaLognormal": "kill_repell", "IRS30": "IRS", "LifeNet1": "LifeNet", "LifeNet2": "LifeNet",
    "MSAT": "MDA", "MSAT_HRP2": "MDA", "PEV": "Vaccine", "Rach5IC": "ITN", "SubPopRemoval": "never symptomatic",
    "TBV": "Vaccine", "TrapTest": "ITN", "TriggeredMSAT": "MSAT", "VecFullTest": "ITN",
}
OTHER = {
    "1": "changeHS", "4": "changeHS", "EffectiveDrug": "changeHS", "Genotypes": "changeHS",
    "KK_20150612": "changeHS", "Molineaux": "changeHS", "InfectionOrigin": "importedInfections",
    "NoMPDLarviciding": "vectorPop", "SimpleMPDLarviciding": "vectorPop", "VecMonthly": "vectorPop",
    "NonHumanHosts": "nonHumanHostsModifications",
}

out = Path(sys.argv[1])
out.mkdir(parents=True, exist_ok=True)
for name in sorted(HUMAN | OTHER):
    src = ROOT / "test" / f"scenario{name}.xml"
    tree = etree.parse(str(src))
    interventions = tree.getroot().find("interventions")
    removed = 0
    if name in HUMAN:
        for deployment in interventions.find("human").findall("deployment"):
            refs = deployment.findall("component")
            for ref in refs:
                if ref.get("id") == HUMAN[name]:
                    removed += 1
                    if len(refs) == 1:
                        deployment.getparent().remove(deployment)
                    else:
                        deployment.remove(ref)
        what = f"human component {HUMAN[name]}"
    else:
        element = interventions.find(OTHER[name])
        interventions.remove(element)
        removed = 1
        what = OTHER[name]
    assert removed, name
    tree.write(str(out / f"scenario{name}.xml"), xml_declaration=True, encoding=tree.docinfo.encoding)
    print(f"{name}: removed {what} ({removed} deployment refs)")
