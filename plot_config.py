import subprocess
import json
import csv
import random
import math
import os
from itertools import product
def make_sphere(i, num_spheres):
    random.seed(i)
    radius = round(random.random(), 2)
    #if i < NUM_DYNAMIC:
    num_dynamic = math.ceil(num_spheres * DYNAMIC_RATIO)
    if i < num_dynamic:
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
    scene += [make_sphere(i, num_spheres) for i in range(num_spheres)]
    return {
        "schema":"scene",
        "resolution": [width, height],
        "antialiasing": 1,
        "camera":{"pos":[4, 1.5, 2], "fov":45},
        "scene": scene
    }
def make_runtime(rt_name, partition=False, buffer=False, layered=False):
    return {
        "schema": "runtime",
        "target": "gpu",
        "gpu_tweaks": {
            "block_width":       16,
            "block_height":      16,
            "partition_objects": partition,
            "buffer_objects":    buffer,
            "layered":           layered
        }
    }


def runtime_name(partition, buffer, layered):
    flags = []
    if partition: flags.append("partition")
    if buffer:    flags.append("buffer")
    if layered:   flags.append("layered")
    return "gpu_" + "_".join(flags) if flags else "gpu_base"


GPU_COMBOS   = [(p, b, l) for p, b, l in product([False, True], repeat=3)]
GPU_RUNTIMES = [runtime_name(*combo) for combo in GPU_COMBOS]

GPU_LABELS = {
    "gpu_base":                    "Base",
    "gpu_partition":               "Partition",
    "gpu_buffer":                  "Buffer",
    "gpu_layered":                 "Layered",
    "gpu_partition_buffer":        "Partition + Buffer",
    "gpu_partition_layered":       "Partition + Layered",
    "gpu_buffer_layered":          "Buffer + Layered",
    "gpu_partition_buffer_layered":"All Three",
}

GPU_COLORS = {
    "gpu_base":                    "#1f77b4",
    "gpu_partition":               "#ff7f0e",
    "gpu_buffer":                  "#2ca02c",
    "gpu_layered":                 "#d62728",
    "gpu_partition_buffer":        "#9467bd",
    "gpu_partition_layered":       "#8c564b",
    "gpu_buffer_layered":          "#e377c2",
    "gpu_partition_buffer_layered":"#17becf",
}

RESOLUTIONS   = [(320, 180), (640, 360), (1280, 720), (1920, 1080)]
RES_COLORS   = ["#1f77b4", "#ff7f0e", "#2ca02c", "#9467bd"]
RES_COLOR_MAP = dict(zip(RESOLUTIONS, RES_COLORS))
SPHERE_COUNTS = [1, 5, 10, 25, 50, 100, 200, 500]

MAX_CPU_SPHERES = 25

GPU_COMBOS = [
    (p, b, l)
    for p, b, l in product([False, True], repeat=3)
]

RUNS_PER_GPU_CONFIG  = 10
RUNS_PER_CPU_CONFIG = 3

BINARY = "./raytracer"
#NUM_DYNAMIC = 100
DYNAMIC_RATIO = .2
MAX_CPU_SPHERES = 25