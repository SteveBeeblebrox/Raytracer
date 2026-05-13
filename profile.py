#!/home/andromeda/Raytracer/venv/bin/python3
import subprocess
import json
import csv
import random
import math
import os
from itertools import product
from plot_config import GPU_COMBOS, GPU_RUNTIMES, RESOLUTIONS, SPHERE_COUNTS, MAX_CPU_SPHERES, BINARY, DYNAMIC_RATIO, RUNS_PER_CPU_CONFIG, RUNS_PER_GPU_CONFIG, make_sphere, make_config, make_runtime, runtime_name

OUTPUT_CSV  = "profile_results.csv"

#https://docs.nvidia.com/nsight-compute/ProfilingGuide/index.html#metrics-structure
#https://docs.nvidia.com/nsight-compute/ProfilingGuide/index.html#launch-metrics
#ncu -query-metrics
#had some LLM help to find and figure out how the metrics all worked

METRICS = ",".join([
    "sm__warps_active.avg.pct_of_peak_sustained_active",  # occupancy-ish?, average percentage of max possble warps that were active on SM
    "dram__throughput.avg.pct_of_peak_sustained_elapsed", # util of DRAM compared to max bandwidth
    "lts__t_sector_hit_rate.pct",                         # L2 hit rate
    "l1tex__t_sector_hit_rate.pct",                       # L1 hit rate
    "gpu__time_duration.sum",                             # Literally just timing the kernels themselves

    "smsp__warps_issue_stalled_long_scoreboard.avg.pct_of_peak_sustained_active", # how much are you waiting on memory
    "smsp__warps_issue_stalled_math_pipe_throttle.avg.pct_of_peak_sustained_active", # how much are you waiting on math
    "smsp__thread_inst_executed_per_inst_executed.avg",  #Warp efficiency? number of active threads per warp instruction, also only ever get 0
    "smsp__sass_average_branch_targets_threads_uniform.avg" #divergance? Only evevr get 0 for it though
])
os.makedirs("_bench_configs",  exist_ok=True)
os.makedirs("_bench_runtimes", exist_ok=True)

import csv
import io

def parse_ncu_csv(text):
    metrics = {}
    # Find the start of the actual CSV data by looking for the ID column
    lines = text.splitlines()
    csv_start_index = -1
    for i, line in enumerate(lines):
        if line.startswith('"ID"') or "Metric Name" in line:
            csv_start_index = i
            break
    
    if csv_start_index == -1:
        return metrics

    # Parse only the valid CSV portion
    csv_data = "\n".join(lines[csv_start_index:])
    reader = csv.DictReader(io.StringIO(csv_data))
    
    for row in reader:
        name = row.get("Metric Name")
        val = row.get("Metric Value")
        
        if name and val:
            try:
                # ncu uses locale-dependent formatting; strip commas
                clean_val = val.replace(",", "")
                metrics[name] = float(clean_val)
            except ValueError:
                continue
    return metrics

def parse_kernel_time(stderr_text):
    """
    Parse kernel GPU time from the standard nvprof summary (non-metrics section).
    Looks for lines like:
      GPU activities:  99.50%  1.23ms  1  1.23ms  ...  kernelName
    Returns total GPU activity time in ms.
    """
    total_ms = None
    for line in stderr_text.splitlines():
        if "GPU activities" in line or "cuda" in line.lower():
            try:
                parts = line.split()
                for i, p in enumerate(parts):
                    if p.endswith("ms"):
                        total_ms = float(p[:-2])
                        break
                    elif p.endswith("us"):
                        total_ms = float(p[:-2]) / 1000
                        break
                if total_ms:
                    break
            except (ValueError, IndexError):
                continue
    return total_ms

with open(OUTPUT_CSV, "w", newline="") as csvfile:
    writer = csv.writer(csvfile)
    # writer.writerow([
    #     "num_spheres", "width", "height", "runtime",
    #     "kernel_time_ms",
    #     "achieved_occupancy",
    #     "gld_throughput", "gst_throughput",
    #     "dram_read_throughput", "dram_write_throughput",
    #     "l2_read_hit_rate", "l1_cache_global_hit_rate",
    # ])
    writer.writerow([
        "num_spheres", "width", "height", "runtime",
        "kernel_time_ms",
        "sm__warps_active.avg.pct_of_peak_sustained_active",
        "dram__throughput.avg.pct_of_peak_sustained_elapsed",
        "lts__t_sector_hit_rate.pct",
        "l1tex__t_sector_hit_rate.pct",
        "smsp__warps_issue_stalled_long_scoreboard.avg.pct_of_peak_sustained_active",
        "smsp__warps_issue_stalled_math_pipe_throttle.avg.pct_of_peak_sustained_active",
        "sm__inst_executed_pipe_fp32.avg.pct_of_peak_sustained_active",
        "smsp__thread_inst_executed_per_inst_executed.avg",
        "smsp__sass_average_branch_targets_threads_uniform.avg"
    ])

    for (p, b, l), rt in zip(GPU_COMBOS, GPU_RUNTIMES):
        rt_json = make_runtime(rt, partition=p, buffer=b, layered=l)
        rt_path = f"_bench_runtimes/{rt}.json"
        with open(rt_path, "w") as f:
            json.dump(rt_json, f)

        for n_spheres in SPHERE_COUNTS:
            for (w, h) in RESOLUTIONS:
                config  = make_config(n_spheres, w, h)
                cfg_path = f"_bench_configs/s{n_spheres}_{w}x{h}.json"
                with open(cfg_path, "w") as f:
                    json.dump(config, f)

                print(f"  Profiling {rt} | {n_spheres} spheres | {w}x{h}...")

                # result = subprocess.run(
                #     ["nvprof", "--csv", f"--metrics", METRICS,
                #      BINARY, cfg_path, rt_path],
                #     input="r\nq\n",
                #     capture_output=True,
                #     text=True
                # )
                env = os.environ.copy()

                tmpdir = os.path.expanduser("~/.ncu_tmp")
                os.makedirs(tmpdir, exist_ok=True)

                env["TMPDIR"] = tmpdir
                env["TEMP"]   = tmpdir
                env["TMP"]    = tmpdir
                result = subprocess.run(
                    [
                        "ncu",
                        "--csv",
                        "--metrics", METRICS,
                        "--target-processes", "all",
                        BINARY, cfg_path, rt_path
                    ],
                    env = env,
                    input="r\nq\n",
                    capture_output=True,
                    text=True
                )
                # metrics = parse_nvprof_csv(result.stderr)
                metrics = parse_ncu_csv(result.stdout)
                #print(metrics.keys())
                #kernel_ms = parse_kernel_time(result.stderr)
                kernel_ms = metrics.get("gpu__time_duration.sum", 0) / 1e6

                if not metrics:
                    print(f"    WARNING: no metrics parsed")
                    print(f"    stderr snippet: {result.stdout[:300]}")
                    continue

                writer.writerow([
                    n_spheres, w, h, rt,
                    kernel_ms,
                    metrics.get("sm__warps_active.avg.pct_of_peak_sustained_active", 0),
                    metrics.get("dram__throughput.avg.pct_of_peak_sustained_elapsed", 0),
                    metrics.get("lts__t_sector_hit_rate.pct", 0),
                    metrics.get("l1tex__t_sector_hit_rate.pct", 0),
                    metrics.get("smsp__warps_issue_stalled_long_scoreboard.avg.pct_of_peak_sustained_active", 0),
                    metrics.get("smsp__warps_issue_stalled_math_pipe_throttle.avg.pct_of_peak_sustained_active", 0),
                    metrics.get("sm__inst_executed_pipe_fp32.avg.pct_of_peak_sustained_active", 0),
                    metrics.get("smsp__thread_inst_executed_per_inst_executed.avg",0),
                    metrics.get("smsp__sass_average_branch_targets_threads_uniform.avg",0)
                ])
                csvfile.flush()
                print(f"    Occ: {metrics.get('sm__warps_active.avg.pct_of_peak_sustained_active', 0):.2f}% | "
                    f"L2: {metrics.get('lts__t_sector_hit_rate.pct', 0):.2f}% | "
                    f"Time: {kernel_ms:.4f}ms")

print(f"\nDone! Results saved to {OUTPUT_CSV}")