#!/bin/bash

set -u

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TOOLS_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

CHIP="${CHIP:-0}"
CORE_ID="${CORE_ID:-0}"
SIZE="${SIZE:-0x1000}"
CDMA_SIZE="${CDMA_SIZE:-0x100000}"
GDMA_SIZE="${GDMA_SIZE:-0x100000}"
SRC_ADDR="${SRC_ADDR:-0}"
DST_ADDR="${DST_ADDR:-0}"
PARAM_NUM="${PARAM_NUM:-1}"
GROUP_NUM="${GROUP_NUM:-1}"
BLOCK_NUM="${BLOCK_NUM:-4}"
RESTORE_TIMEOUT_MS="${RESTORE_TIMEOUT_MS:-120000}"
TEST_SYNC_TIMEOUT_MS="${TEST_SYNC_TIMEOUT_MS:-1000}"
CDMA_BOARD_DIR="${CDMA_BOARD_DIR:-1}"
CDMA_BOARD_LAUNCH="${CDMA_BOARD_LAUNCH:-10}"
MODULE_SO="${MODULE_SO:-}"
BIN_DIR="${BIN_DIR:-}"
LOG_DIR="${LOG_DIR:-}"
SAVE_LOG=0
RUN_DANGEROUS=0
RUN_PROFILE=0
SELECTED_GROUPS=""
LIST_ONLY=0

PASS_CNT=0
FAIL_CNT=0
PASS_LIST=()
FAIL_LIST=()
SUB_PASS=0
SUB_FAIL=0
SUB_OPTIONAL=0

resolve_bin_dir() {
	if [ -n "${BIN_DIR}" ]; then
		return 0
	fi
	if command -v test_get_fw_version >/dev/null 2>&1; then
		BIN_DIR="$(dirname "$(command -v test_get_fw_version)")"
		return 0
	fi
	if [ -x "/opt/sophon/libsophon-current/bin/test_get_fw_version" ]; then
		BIN_DIR="/opt/sophon/libsophon-current/bin"
		return 0
	fi
	if [ -x "${TOOLS_DIR}/test_get_fw_version" ]; then
		BIN_DIR="${TOOLS_DIR}"
		return 0
	fi
	BIN_DIR="/opt/sophon/libsophon-current/bin"
}

find_bin() {
	local name="$1"
	if [ -x "${BIN_DIR}/${name}" ]; then
		echo "${BIN_DIR}/${name}"
		return 0
	fi
	if command -v "${name}" >/dev/null 2>&1; then
		command -v "${name}"
		return 0
	fi
	return 1
}

detect_chipid() {
	local f id out bin
	for f in /proc/bmsophon/card*/chipid; do
		if [ -r "${f}" ]; then
			tr -d '\n' < "${f}"
			return 0
		fi
	done
	for f in /proc/*/bmdi_base_info; do
		if [ -r "${f}" ]; then
			id="$(sed -n 's/.*chip_id:\(0x[0-9a-fA-F]*\).*/\1/p' "${f}" | head -n 1)"
			if [ -n "${id}" ]; then
				echo "${id}"
				return 0
			fi
		fi
	done
	if [ -n "${BIN_DIR}" ] && [ -x "${BIN_DIR}/test_bmlib_apis" ]; then
		bin="${BIN_DIR}/test_bmlib_apis"
	elif command -v test_bmlib_apis >/dev/null 2>&1; then
		bin="$(command -v test_bmlib_apis)"
	else
		bin=""
	fi
	if [ -n "${bin}" ]; then
		out="$("${bin}" "${CHIP}" info 2>/dev/null || true)"
		id="$(printf '%s\n' "${out}" | sed -n 's/.*chipid=0x\([0-9a-fA-F]*\).*/0x\1/p' | head -n 1)"
		if [ -n "${id}" ]; then
			echo "${id}"
			return 0
		fi
	fi
	if [ -r /proc/config.gz ] && zcat /proc/config.gz 2>/dev/null | grep -q '^CONFIG_CHIP_CPU_CV84X6=y'; then
		echo "0x1694"
		return 0
	fi
	id="$(dmesg 2>/dev/null | sed -n 's/.*current chipid is \(0x[0-9a-fA-F]*\).*/\1/p' | tail -n 1)"
	if [ -n "${id}" ]; then
		echo "${id}"
		return 0
	fi
	return 1
}

find_default_module() {
	local chipid=""
	local candidates=()
	local p
	chipid="$(detect_chipid || true)"
	if [ "${chipid}" = "0x1694" ]; then
		candidates=(
			"/lib/firmware/libfirmware_core.so"
			"/opt/sophon/libsophon-current/lib/tpu_module/libfirmware_core.so"
			"./libfirmware_core.so"
		)
	else
		candidates=(
			"/lib/firmware/libbm1688_kernel_module.so"
			"/opt/sophon/libsophon-current/lib/tpu_module/libbm1688_kernel_module.so"
			"./libbm1688_kernel_module.so"
		)
	fi
	for p in "${candidates[@]}"; do
		if [ -f "${p}" ]; then
			echo "${p}"
			return 0
		fi
	done
	return 1
}

print_usage() {
	cat <<EOF
Usage: $(basename "$0") [options] [groups...]

Groups:
  basic       device info / fw version / malloc / sync
  api         uncovered APIs: info/alloc/partial/memset/d2d/mmap/sync
  mem         CDMA s2d/d2s / board / u64 / ion heap memcpy
  gdma        GDMA d2d / memcpy_d2d / launch_multicores
  a53lite     a53lite load/launch/gdma tests (need MODULE_SO)
  profile     run one gdma case (no BMLIB_ENABLE_ALL_PROFILE)
  extra       2d cdma
  dangerous   reset (OFF by default, pass --dangerous)
  all         basic + api + mem + gdma + a53lite + profile + extra
  smoke       basic + api + mem + gdma

Options:
  -h, --help              show help
  -l, --list              list cases only
  -d, --bin-dir DIR       binary directory
  -c, --chip N            chip index (default: ${CHIP})
  -s, --size HEX          transfer size for gdma/multicores (default: ${SIZE})
  --cdma-size HEX         transfer size for cdma (default: ${CDMA_SIZE})
  --gdma-size HEX         transfer size for gdma (default: ${GDMA_SIZE})
  --core N                core id (default: ${CORE_ID})
  --param-num N           launch_multicores param_num 1|2 (default: ${PARAM_NUM})
  --module SO             firmware/module path for a53lite tests
  --log-dir DIR           save logs to DIR (default: no log file)
  --dangerous             allow dangerous group
  --profile               force enable profile group

Env overrides:
  CHIP CORE_ID SIZE CDMA_SIZE GDMA_SIZE SRC_ADDR DST_ADDR
  PARAM_NUM MODULE_SO BIN_DIR LOG_DIR GROUP_NUM BLOCK_NUM
  RESTORE_TIMEOUT_MS TEST_SYNC_TIMEOUT_MS

Examples:
  $(basename "$0")                 # show this help
  $(basename "$0") smoke
  $(basename "$0") basic gdma
  $(basename "$0") all --module /path/to/a53lite_test.so
  $(basename "$0") smoke --log-dir ./bmlib_test_logs
EOF
}

parse_case_summary() {
	local out_file="$1"
	local line
	local p f w

	line="$(grep -E '^BMLIB_CASE_SUMMARY ' "${out_file}" 2>/dev/null | tail -n 1 || true)"
	if [ -n "${line}" ]; then
		p="$(echo "${line}" | sed -n 's/.*pass=\([0-9][0-9]*\).*/\1/p')"
		f="$(echo "${line}" | sed -n 's/.*fail=\([0-9][0-9]*\).*/\1/p')"
		w="$(echo "${line}" | sed -n 's/.*warn=\([0-9][0-9]*\).*/\1/p')"
		SUB_PASS=$((SUB_PASS + ${p:-0}))
		SUB_FAIL=$((SUB_FAIL + ${f:-0}))
		SUB_OPTIONAL=$((SUB_OPTIONAL + ${w:-0}))
		return 0
	fi

	line="$(grep -E '^TOTAL=[0-9]+ PASS=[0-9]+ FAIL=[0-9]+ OPTIONAL=[0-9]+' "${out_file}" 2>/dev/null | tail -n 1 || true)"
	if [ -n "${line}" ]; then
		p="$(echo "${line}" | sed -n 's/.*PASS=\([0-9][0-9]*\).*/\1/p')"
		f="$(echo "${line}" | sed -n 's/.*FAIL=\([0-9][0-9]*\).*/\1/p')"
		w="$(echo "${line}" | sed -n 's/.*OPTIONAL=\([0-9][0-9]*\).*/\1/p')"
		SUB_PASS=$((SUB_PASS + ${p:-0}))
		SUB_FAIL=$((SUB_FAIL + ${f:-0}))
		SUB_OPTIONAL=$((SUB_OPTIONAL + ${w:-0}))
		return 0
	fi

	line="$(grep -E '^PASS=[0-9]+ FAIL=[0-9]+ WARN=[0-9]+' "${out_file}" 2>/dev/null | tail -n 1 || true)"
	if [ -n "${line}" ]; then
		p="$(echo "${line}" | sed -n 's/PASS=\([0-9][0-9]*\).*/\1/p')"
		f="$(echo "${line}" | sed -n 's/.*FAIL=\([0-9][0-9]*\).*/\1/p')"
		w="$(echo "${line}" | sed -n 's/.*WARN=\([0-9][0-9]*\).*/\1/p')"
		SUB_PASS=$((SUB_PASS + ${p:-0}))
		SUB_FAIL=$((SUB_FAIL + ${f:-0}))
		SUB_OPTIONAL=$((SUB_OPTIONAL + ${w:-0}))
	fi
}

run_one() {
	local name="$1"
	shift
	local bin=""
	local log_file=""
	local out_file=""
	local rc=0

	if ! bin="$(find_bin "${name}")"; then
		echo "[FAIL] ${name} (binary not found)"
		FAIL_CNT=$((FAIL_CNT + 1))
		FAIL_LIST+=("${name}")
		return 0
	fi

	out_file="$(mktemp)"
	echo "------------------------------------------------------------"
	echo "[RUN ] ${name} $*"
	echo "cmd: ${bin} $*"
	set +e
	if [ "${SAVE_LOG}" -eq 1 ]; then
		mkdir -p "${LOG_DIR}"
		log_file="${LOG_DIR}/${name}.log"
		"${bin}" "$@" 2>&1 | tee "${out_file}" | tee "${log_file}"
		rc=${PIPESTATUS[0]}
	else
		"${bin}" "$@" 2>&1 | tee "${out_file}"
		rc=${PIPESTATUS[0]}
	fi
	set -u
	parse_case_summary "${out_file}"
	rm -f "${out_file}"

	if [ ${rc} -eq 0 ]; then
		echo "[PASS] ${name}"
		PASS_CNT=$((PASS_CNT + 1))
		PASS_LIST+=("${name}")
	else
		if [ "${SAVE_LOG}" -eq 1 ]; then
			echo "[FAIL] ${name} (rc=${rc}), log: ${log_file}"
		else
			echo "[FAIL] ${name} (rc=${rc})"
		fi
		FAIL_CNT=$((FAIL_CNT + 1))
		FAIL_LIST+=("${name}")
	fi
	return 0
}

group_basic() {
	run_one test_get_fw_version
	run_one test_get_stat
	run_one test_malloc_time
	run_one test_set_sync_time "${TEST_SYNC_TIMEOUT_MS}"
}

group_api() {
	run_one test_bmlib_apis "${CHIP}" all
}

group_mem() {
	run_one test_cdma_perf chip "${CHIP}" "${CDMA_SIZE}" "${DST_ADDR}"
	run_one test_cdma_perf_u64
	run_one test_cdma_board "${CHIP}" "${CDMA_SIZE}" "${CDMA_BOARD_DIR}" "${CDMA_BOARD_LAUNCH}"
	run_one test_ion_heap_memcpy "${CHIP}"
	run_one test_gmem_flush "${CHIP}"
}

group_gdma() {
	run_one test_gdma_perf d2d "${CHIP}" "${CORE_ID}" "${GDMA_SIZE}" "${SRC_ADDR}" "${DST_ADDR}"
	run_one test_memcpy_d2d d2d "${CHIP}" "${CORE_ID}" "${GDMA_SIZE}" "${SRC_ADDR}" "${DST_ADDR}"
	run_one test_launch_multicores "${CHIP}" "${SIZE}" "${PARAM_NUM}"
}

group_a53lite() {
	if [ -z "${MODULE_SO}" ] || [ ! -f "${MODULE_SO}" ]; then
		return 0
	fi
	local base
	base="$(basename "${MODULE_SO}")"
	if [ "${base}" = "libfirmware_core.so" ] || [ "${base}" = "libbm1688_kernel_module.so" ]; then
		return 0
	fi
	run_one a53lite_dl_test_key "${CHIP}" "${MODULE_SO}" "${base}"
	run_one a53lite_gdma_loadfile "${CHIP}" "${MODULE_SO}"
	run_one a53lite_gdma_loadmod "${CHIP}" "${MODULE_SO}"
	run_one a53lite_gdma_multithread "${CHIP}" "${MODULE_SO}" 2
	run_one a53lite_memcpy_test "${CHIP}" "${MODULE_SO}" "${GROUP_NUM}" "${BLOCK_NUM}"
	run_one a53lite_repeat_loadlib "${CHIP}" "${MODULE_SO}"
}

group_profile() {
	run_one test_gdma_perf d2d "${CHIP}" "${CORE_ID}" "${SIZE}" "${SRC_ADDR}" "${DST_ADDR}"
}

group_extra() {
	run_one test_2d_cdma_perf
}

group_dangerous() {
	if [ "${RUN_DANGEROUS}" -ne 1 ]; then
		return 0
	fi
	run_one test_reset "${CHIP}"
	run_one test_reset_tpu_a2 "${CHIP}"
}

list_cases() {
	cat <<EOF
basic:
  test_get_fw_version
  test_get_stat
  test_malloc_time

api:
  test_bmlib_apis all
  test_bmlib_apis info|alloc|partial|memset|d2d|mmap|sync

mem:
  test_cdma_perf chip
  test_cdma_perf_u64
  test_cdma_board
  test_ion_heap_memcpy
  test_gmem_flush

gdma:
  test_gdma_perf d2d
  test_memcpy_d2d d2d
  test_launch_multicores

a53lite:
  need --module <a53lite_test.so> (not libfirmware_core.so)
  a53lite_dl_test_key / gdma_* / memcpy / repeat_loadlib

profile:
  test_gdma_perf d2d (SIZE, no BMLIB_ENABLE_ALL_PROFILE)

extra:
  test_2d_cdma_perf

dangerous:
  test_reset
  test_reset_tpu_a2
EOF
}

run_group() {
	case "$1" in
		basic) group_basic ;;
		api) group_api ;;
		mem) group_mem ;;
		gdma) group_gdma ;;
		a53lite) group_a53lite ;;
		profile) group_profile ;;
		extra) group_extra ;;
		dangerous) group_dangerous ;;
		smoke)
			group_basic
			group_api
			group_mem
			group_gdma
			;;
		all)
			group_basic
			group_api
			group_mem
			group_gdma
			group_a53lite
			group_profile
			group_extra
			group_dangerous
			;;
		*)
			echo "unknown group: $1"
			print_usage
			exit 2
			;;
	esac
}

restore_sync_timeout() {
	echo "######## restore sync timeout ########"
	run_one test_set_sync_time "${RESTORE_TIMEOUT_MS}"
}

print_summary() {
	local prog_total=$((PASS_CNT + FAIL_CNT))
	local sub_total=$((SUB_PASS + SUB_FAIL + SUB_OPTIONAL))
	local grand_total=$((prog_total + sub_total))
	local grand_pass=$((PASS_CNT + SUB_PASS))
	local grand_fail=$((FAIL_CNT + SUB_FAIL))

	echo "============================================================"
	echo "BIN_DIR=${BIN_DIR}"
	echo "CHIP=${CHIP} CORE_ID=${CORE_ID} SIZE=${SIZE} CDMA_SIZE=${CDMA_SIZE} GDMA_SIZE=${GDMA_SIZE}"
	echo "------------------------------------------------------------"
	echo "Programs (executables):"
	echo "  TOTAL:    ${prog_total}"
	echo "  PASS:     ${PASS_CNT}"
	echo "  FAIL:     ${FAIL_CNT}"
	if [ ${#PASS_LIST[@]} -gt 0 ]; then
		echo "  PASS list: ${PASS_LIST[*]}"
	fi
	if [ ${#FAIL_LIST[@]} -gt 0 ]; then
		echo "  FAIL list: ${FAIL_LIST[*]}"
	fi
	if [ ${sub_total} -gt 0 ]; then
		echo "------------------------------------------------------------"
		echo "API checks (test_bmlib_apis sub-cases):"
		echo "  TOTAL:    ${sub_total}"
		echo "  PASS:     ${SUB_PASS}"
		echo "  FAIL:     ${SUB_FAIL}"
		echo "  OPTIONAL: ${SUB_OPTIONAL}"
	fi
	echo "------------------------------------------------------------"
	echo "Grand total (programs + API checks):"
	echo "  TOTAL:    ${grand_total}"
	echo "  PASS:     ${grand_pass}"
	echo "  FAIL:     ${grand_fail}"
	if [ "${SAVE_LOG}" -eq 1 ]; then
		echo "logs: ${LOG_DIR}"
	fi
	echo "============================================================"
}

while [ $# -gt 0 ]; do
	case "$1" in
		-h|--help)
			print_usage
			exit 0
			;;
		-l|--list)
			LIST_ONLY=1
			shift
			;;
		-d|--bin-dir)
			BIN_DIR="$2"
			shift 2
			;;
		-c|--chip)
			CHIP="$2"
			shift 2
			;;
		-s|--size)
			SIZE="$2"
			shift 2
			;;
		--cdma-size)
			CDMA_SIZE="$2"
			shift 2
			;;
		--gdma-size)
			GDMA_SIZE="$2"
			shift 2
			;;
		--core)
			CORE_ID="$2"
			shift 2
			;;
		--param-num)
			PARAM_NUM="$2"
			shift 2
			;;
		--module)
			MODULE_SO="$2"
			shift 2
			;;
		--log-dir)
			LOG_DIR="$2"
			SAVE_LOG=1
			shift 2
			;;
		--dangerous)
			RUN_DANGEROUS=1
			shift
			;;
		--profile)
			RUN_PROFILE=1
			shift
			;;
		--)
			shift
			break
			;;
		-*)
			echo "unknown option: $1"
			print_usage
			exit 2
			;;
		*)
			SELECTED_GROUPS="${SELECTED_GROUPS} $1"
			shift
			;;
	esac
done

if [ "${LIST_ONLY}" -eq 1 ]; then
	list_cases
	exit 0
fi

resolve_bin_dir

if [ -n "${LOG_DIR}" ]; then
	SAVE_LOG=1
fi

if [ -z "${SELECTED_GROUPS// /}" ]; then
	print_usage
	exit 0
fi
if [ "${RUN_PROFILE}" -eq 1 ]; then
	SELECTED_GROUPS="${SELECTED_GROUPS} profile"
fi

echo "BIN_DIR=${BIN_DIR}"
echo "groups:${SELECTED_GROUPS}"
if [ "${SAVE_LOG}" -eq 1 ]; then
	mkdir -p "${LOG_DIR}"
	echo "log_dir=${LOG_DIR}"
fi

for g in ${SELECTED_GROUPS}; do
	echo "######## group: ${g} ########"
	run_group "${g}"
done

restore_sync_timeout

print_summary

if [ "${FAIL_CNT}" -gt 0 ] || [ "${SUB_FAIL}" -gt 0 ]; then
	exit 1
fi
exit 0
