#!/home/andromeda/Raytracer/venv/bin/python3
import subprocess
import json
import csv
import random
import math
import os
from itertools import product
from plot_config import GPU_COMBOS, GPU_RUNTIMES, RESOLUTIONS, SPHERE_COUNTS, MAX_CPU_SPHERES, BINARY, DYNAMIC_RATIO, RUNS_PER_CPU_CONFIG, RUNS_PER_GPU_CONFIG, make_sphere, make_config, make_runtime, runtime_name



OUTPUT_CSV = "results.csv"

os.makedirs("_bench_configs", exist_ok=True)
os.makedirs("_bench_runtimes", exist_ok=True)

with open(OUTPUT_CSV, "w", newline="") as csvfile:
    writer = csv.writer(csvfile)
    writer.writerow(["num_spheres", "width", "height", "run", "time_ms", "runtime"])

    # CPU first
    cpu_rt_path = "_bench_runtimes/cpu.json"
    with open(cpu_rt_path, "w") as f:
        json.dump({"schema": "runtime", "target": "cpu"}, f)

    for n_spheres in SPHERE_COUNTS:
        #limit the amount of spheres the CPU can use because it is veryyyyyyyy slow
        if n_spheres > MAX_CPU_SPHERES:
            break
        for (w, h) in RESOLUTIONS:
            config = make_config(n_spheres, w, h)
            cfg_path = f"_bench_configs/s{n_spheres}_{w}x{h}.json"
            with open(cfg_path, "w") as f:
                json.dump(config, f)
            for run in range(RUNS_PER_CPU_CONFIG):
                result = subprocess.run(
                    [BINARY, cfg_path, cpu_rt_path],
                    input="r\nq\n", capture_output=True, text=True
                )
                time_ms = None
                for line in result.stdout.splitlines():
                    if line.startswith("ms_time:"):
                        time_ms = float(line.split(":")[1])
                        break
                if time_ms is None:
                    print(f"  WARNING: no timing for cpu | {n_spheres} spheres @ {w}x{h} run {run}")
                    print("  stderr:", result.stderr[:200])
                else:
                    print(f"  {'cpu':30s} | {n_spheres:4d} spheres | {w}x{h} | run {run} | {time_ms:.1f} ms")
                    writer.writerow([n_spheres, w, h, run, time_ms, "cpu"])
                    csvfile.flush()

    # do all the GPU combos
    for (p, b, l), rt in zip(GPU_COMBOS, GPU_RUNTIMES):
        #just regen runtimes and config each time since it doesnt take long and makes it easier to make changes to the runtimes/configs between runs
        rt_json = make_runtime(rt, partition=p, buffer=b, layered=l)
        rt_path = f"_bench_runtimes/{rt}.json"
        with open(rt_path, "w") as f:
            json.dump(rt_json, f)

        for n_spheres in SPHERE_COUNTS:
            for (w, h) in RESOLUTIONS:
                config = make_config(n_spheres, w, h)
                cfg_path = f"_bench_configs/s{n_spheres}_{w}x{h}.json"
                with open(cfg_path, "w") as f:
                    json.dump(config, f)
                for run in range(RUNS_PER_GPU_CONFIG):
                    result = subprocess.run(
                        [BINARY, cfg_path, rt_path],
                        input="r\nq\n", capture_output=True, text=True
                    )
                    time_ms = None
                    for line in result.stdout.splitlines():
                        if line.startswith("ms_time:"):
                            time_ms = float(line.split(":")[1])
                            break
                    if time_ms is None:
                        print(f"  WARNING: no timing for {rt} | {n_spheres} spheres @ {w}x{h} run {run}")
                        print("  stderr:", result.stderr[:200])
                    else:
                        print(f"  {rt:30s} | {n_spheres:4d} spheres | {w}x{h} | run {run} | {time_ms:.1f} ms")
                        writer.writerow([n_spheres, w, h, run, time_ms, rt])
                        csvfile.flush()

print(f"\nDone! Results saved to {OUTPUT_CSV}")