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



# job_script.sh with GNU parallel, one job per core by default.
#
#   ./wrapper.sh ../params/nbiot_input_ambient.csv
#   JOBS=18 ./wrapper.sh ../params/nbiot_input_stress.csv
#
# Re-running the same command RESUMES: completed rows (per the joblog) are
# skipped, failed/missing rows re-run. Delete the .log for a fresh start.
#



if [[ $# -ne 1 ]] ; then
  echo "# Usage: $0 <input.csv>"
  exit -1
elif [[ ! -r $1 ]] ; then
  echo "# Error: input "$1" is not a readable file"
  exit -1
fi

CSV="$1"
NAME="${CSV%.csv}"
LOG="${NAME}.log"
RUN="$(dirname "${BASH_SOURCE[0]}")/job_script.sh"
JOBS=${JOBS:-$(nproc)}

if [[ ! -x ${RUN} ]] ; then
  echo "# Error: run file "${RUN}" must be an executable"
  exit -1
fi

echo "# Input file: ${CSV}"
echo "# Run script: ${RUN}"
echo "# Log file:   ${LOG}"
echo "# Parallel:   ${JOBS} jobs"

HEADER=$(head -1 "${CSV}")
parallel -P ${JOBS} -a ${CSV} --resume-failed --ungroup --tag --joblog ${LOG} --header '.*\n' ${RUN} ${HEADER}
exitcode=$?

if [[ $exitcode == 0 ]] ; then
  echo "# Result: all $(($(wc -l ${CSV} | awk '{print $1}') - 1)) job steps ran successfully"
else
  missing=$(parallel --dry-run -a ${CSV} --resume-failed --joblog ${LOG} --header : ${RUN} ${HEADER} | wc -l)
  echo "# Result: some jobs failed, still need to run $missing job steps"
  exit $exitcode
fi
