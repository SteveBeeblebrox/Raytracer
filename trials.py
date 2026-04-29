import subprocess
import json
import csv
import random
import math
import os

BINARY = "./raytracer"
OUTPUT_CSV = "results.csv"
def make_sphere(i):
    random.seed(i)
    return {
        "type": "sphere",
        "pos[0]": round(random.random(), 2),
        "pos[1]": round(random.random(), 2),
        "pos[2]": round(random.random(), 2),
        "radius": round(random.random(), 2),
        "material.diffuse[0]": round(random.random(), 2),
        "material.diffuse[1]": round(random.random(), 2),
        "material.diffuse[2]": round(random.random(), 2),
        "material.ambient[0]": round(random.random(), 2),
        "material.ambient[1]": round(random.random(), 2),
        "material.ambient[2]": round(random.random(), 2),
        "material.specular[0]": round(random.random(), 2),
        "material.specular[1]": round(random.random(), 2),
        "material.specular[2]": round(random.random(), 2),
        "material.shininess": round(random.uniform(0, 10), 2),
        "material.reflectivity": round(random.random(), 2),
        "material.refraction": round(random.uniform(0,2), 2),
        "material.alpha": round(random.random(), 2)
    }

def make_config(num_spheres, width, height):
    scene = [
        {
            "type": "light"#,
            # "pos[0]": 5, "pos[1]": 5, "pos[2]": 5,
            # "diffuse[0]": 1, "diffuse[1]": 1, "diffuse[2]": 1,
            # "ambient[0]": 0.2, "ambient[1]": 0.2, "ambient[2]": 0.2,
            # "specular[0]": 1, "specular[1]": 1, "specular[2]": 1
        }
    ]
    scene += [make_sphere(i) for i in range(num_spheres)]
    return {
        "resolution[0]": width,
        "resolution[1]": height,
        "antialiasing": 1,
        "camera": {"pos[0]": 4, "pos[1]": 1.5, "pos[2]": 2, "fov": 45},
        "scene": scene
    }

SPHERE_COUNTS   = [1, 5, 10, 25, 50, 100, 200]
RESOLUTIONS     = [(320, 180), (640, 360), (1280, 720), (1920, 1080)]
RUNS_PER_CONFIG = 3   # average over multiple runs to reduce noise

os.makedirs("_bench_configs", exist_ok=True)

with open(OUTPUT_CSV, "w", newline="") as csvfile:
    writer = csv.writer(csvfile)
    writer.writerow(["num_spheres", "width", "height", "run", "time_ms"])

    for n_spheres in SPHERE_COUNTS:
        for (w, h) in RESOLUTIONS:
            config = make_config(n_spheres, w, h)

            cfg_path = f"_bench_configs/s{n_spheres}_{w}x{h}.json"
            with open(cfg_path, "w") as f:
                json.dump(config, f)

            for run in range(RUNS_PER_CONFIG):
                result = subprocess.run(
                    [BINARY, cfg_path],
                    input="r\nq\n",
                    capture_output=True,
                    text=True
                )

                # parse the TIMING_MS: line from stdout
                time_ms = None
                for line in result.stdout.splitlines():
                    if line.startswith("TIMING_MS:"):
                        time_ms = float(line.split(":")[1])
                        break

                if time_ms is None:
                    print(f"  WARNING: no timing found for {n_spheres} spheres @ {w}x{h} run {run}")
                    print("  stderr:", result.stderr[:200])
                else:
                    print(f"  {n_spheres:4d} spheres | {w}x{h} | run {run} | {time_ms:.1f} ms")
                    writer.writerow([n_spheres, w, h, run, time_ms])
                    csvfile.flush()   # write incrementally in case of crash

print(f"\nDone! Results saved to {OUTPUT_CSV}")