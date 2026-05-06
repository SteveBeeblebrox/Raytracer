#!/usr/bin/env python3
import subprocess
import json
import csv
import random
import math
import os

BINARY = "./raytracer"
OUTPUT_CSV = "results.csv"
NUM_DYNAMIC = 7
MAX_CPU_SPHERES = 25
def make_sphere(i):
    random.seed(i)
    radius = round(random.random(), 2)
    if i < NUM_DYNAMIC:
        radius = "_dynamic"
    return {
        "type": "sphere",
        "pos": [round(random.random(), 2), round(random.random(), 2), round(random.random(), 2)],
        "radius": radius,
        "material":{
            "diffuse": [round(random.random(), 2), round(random.random(), 2), round(random.random(), 2)],
            "ambient": [round(random.random(), 2), round(random.random(), 2), round(random.random(), 2)],
            "specular": [round(random.random(), 2), round(random.random(), 2), round(random.random(), 2)],
            "shininess": round(random.uniform(0, 10), 2),
            "reflectivity": round(random.random(), 2),
            "refraction": round(random.uniform(0,2), 2),
            "alpha": round(random.random(), 2)
        }
    }

def make_config(num_spheres, width, height):
    scene = [
        {
            "type": "light"
        }
    ]
    scene += [make_sphere(i) for i in range(num_spheres)]
    return {
        "schema":"scene",
        "resolution": [width, height],
        "antialiasing": 1,
        "camera":{"pos":[4, 1.5, 2], "fov":45},
        "scene": scene
    }
def make_runtime(rt):
    rt_label = rt
    layered = False
    if rt == "gpu_layered":
        rt_label = "gpu"
        layered = True
    return {
    "schema": "runtime",
    "target": rt_label,
    "gpu_tweaks": {
        "block_width": 16,
        "block_height": 16,
        "partition_objects": True,
        "buffer_objects": True,
        "layered": layered
    }
}

SPHERE_COUNTS   = [1, 5, 10, 25, 50, 100, 200, 500]
RESOLUTIONS     = [(320, 180), (640, 360), (1280, 720), (1920, 1080)]
RUNS_PER_CONFIG = [10, 3] #first is for GPU second is for CPU
RUNTIMES = ["gpu", "cpu"]

os.makedirs("_bench_configs", exist_ok=True)
os.makedirs("_bench_runtimes", exist_ok=True)

with open(OUTPUT_CSV, "w", newline="") as csvfile:
    writer = csv.writer(csvfile)
    writer.writerow(["num_spheres", "width", "height", "run", "time_ms", "runtime"])
    for rti in range(len(RUNTIMES)):
        rt = RUNTIMES[rti]
        runs = RUNS_PER_CONFIG[rti]

        rt_json = make_runtime(rt)
        rt_path = f"_bench_runtimes/{rt}.json"
        with open(rt_path, "w") as f:
            json.dump(rt_json, f)
        for n_spheres in SPHERE_COUNTS:
            if (rt == "cpu") and (n_spheres > MAX_CPU_SPHERES):
                break
            for (w, h) in RESOLUTIONS:
                config = make_config(n_spheres, w, h)

                cfg_path = f"_bench_configs/s{n_spheres}_{w}x{h}.json"
                with open(cfg_path, "w") as f:
                    json.dump(config, f)

                for run in range(runs):
                    result = subprocess.run(
                        [BINARY, cfg_path, rt_path],
                        input="r\nq\n",
                        capture_output=True,
                        text=True
                    )

                    # parse the TIMING_MS: line from stdout
                    time_ms = None
                    for line in result.stdout.splitlines():
                        if line.startswith("ms_time:"):
                            time_ms = float(line.split(":")[1])
                            break

                    if time_ms is None:
                        print(f"  WARNING: no timing found for {n_spheres} spheres @ {w}x{h} run {run}")
                        print("  stderr:", result.stderr[:200])
                    else:
                        print(f"  {n_spheres:4d} spheres | {w}x{h} | run {run} | {time_ms:.1f} ms")
                        writer.writerow([n_spheres, w, h, run, time_ms, rt])
                        csvfile.flush()   # write incrementally in case of crash

print(f"\nDone! Results saved to {OUTPUT_CSV}")