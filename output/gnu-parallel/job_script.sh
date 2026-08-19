#!/bin/bash

# Copyright (c) 2026 IDLab (UAntwerp & imec)
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 3 as
# published by the Free Software Foundation;
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#
# Author: Douglas D. Agbeve <douglas.agbeve@uantwerpen.be>
#


# Invoked by wrapper.sh via GNU parallel as:  ./job_script.sh <csv-header> <csv-row>
# CSV columns: run,numUes,scheme[,harvest]   (same CSVs as gnu-parallel/params/)
#   scheme : RA | idealfug | hybridsr | dedicated | fug
#   harvest: peak harvest in W; column optional -> defaults to 0.015 (15 mW)
#
# DRYRUN=1 ./job_script.sh <header> <row>   prints the command instead of running.
#


read $(echo "$1" | tr ',' ' ') < <(echo $2 | tr ',' ' ')

# Resolve the repo root from this script's location (gnu-parallel).
if [[ -z "${REPO}" ]]; then
  REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
  while [[ "${REPO}" != "/" && ! -x "${REPO}/build/scratch/nb-iot-ambient6g/nb-iot-ambient6g" ]]; do
    REPO="$(dirname "${REPO}")"
  done
fi
BIN=${REPO}/build/scratch/nb-iot-ambient6g/nb-iot-ambient6g
if [[ ! -x "${BIN}" ]]; then
  echo "# Error: built binary not found (searched upward from this script)." >&2
  echo "#        Build first:  ./waf build   (or set REPO=/path/to/repo)" >&2
  exit 1
fi
BIN=${REPO}/build/scratch/nb-iot-ambient6g/nb-iot-ambient6g
OUTBASE=${REPO}/output/output_ambient      # same roots the analysis script reads
export LD_LIBRARY_PATH=${REPO}/build/lib

# Map scheme -> flag block (identical to gnu-parallel/job_script.sh)
case "${scheme}" in
  RA)        MODE="--persistentGrant=false" ;;
  idealfug)  MODE="--persistentGrant=true --deepSleepFug=true --oracleBsr=true" ;;
  hybridsr)  MODE="--persistentGrant=true --deepSleepFug=true --srPreambleSr=true --srHybridContention=true" ;;
  dedicated) MODE="--persistentGrant=true --deepSleepFug=true --srPreambleSr=true --srHybridContention=false" ;;
  fug)       MODE="--proactiveFug=true" ;;
  *) echo "# Error: unknown scheme '${scheme}'" >&2; exit 1 ;;
esac

# Harvest peak in W; legacy 3-column CSVs carry no harvest column -> 15 mW,
# stress rows land in a separate output root per level (same as the cluster).
HARVEST=${harvest:-0.015}
if [[ "${HARVEST}" == "0.015" ]]; then HTAG=""; else HTAG="_h${HARVEST}"; fi

LOGDIR=${OUTBASE}${HTAG}/${scheme}/${run}_run-${numUes}_Ues
mkdir -p ${LOGDIR}

CMD="${BIN} \
    --simDuration=3610 --numUes=${numUes} --RngRun=${run} --sendFirst=false --ns3Debug=false \
    ${MODE} \
    --edt=1 \
    --solarProfile=true --capInitVRandom=true --capInitVMin=2.9 --capMaxV=3.3 \
    --harvestPmaxW=${HARVEST} --statsStartSec=300 --ambientIoT=true \
    --logDir=${LOGDIR}/ \
    --ns3::LteUePhy::RsrpSinrSamplePeriod=1 \
    --ns3::LteUePhy::NoiseFigure=0 \
    --ns3::LteEnbPhy::NoiseFigure=0 \
    --ns3::LteSpectrumPhy::EnableInterference=true"

if [[ -n "${DRYRUN}" ]]; then
  echo "${CMD}"
else
  ${CMD}
fi
