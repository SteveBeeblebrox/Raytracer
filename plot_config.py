import subprocess
import json
import csv
import random
import math
import os
from itertools import product
def make_sphere(i, num_spheres):
    #its easiest to make spheres as needed instead of keeping a global array since I want want logic centered here to be shared with the two trial scripts
    #this is for consistency so the same spheres are generated in the same sequence per trial no matter how the overall trial structure is changed
    random.seed(i)
    radius = round(random.random(), 2)
    #if i < NUM_DYNAMIC:
    #make the first percentage dynamic, I think its more important to have a fair ratio for the partitioning optimization to be at all meaningful
    #using a fixed number messes up the comparison for the partitioning effect between large and small sphere counts
    num_dynamic = math.ceil(num_spheres * DYNAMIC_RATIO)
    if i < num_dynamic:
        radius = "_dynamic"
    #randomize everything :D
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
    #copy of Trin's config file
    scene = [
        {
            "type": "light"
        }
    ]
    #generate spheres
    scene += [make_sphere(i, num_spheres) for i in range(num_spheres)]
    return {
        "schema":"scene",
        "resolution": [width, height],
        "antialiasing": 1,
        "camera":{"pos":[4, 1.5, 2], "fov":45},
        "scene": scene
    }
def make_runtime(rt_name, partition=False, buffer=False, layered=False):
    #copy of Trin's runtime file just with passed in flags
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
    #just generate the runtime names based on the optimizations being used for simplicity
    flags = []
    if partition: flags.append("partition")
    if buffer:    flags.append("buffer")
    if layered:   flags.append("layered")
    #denote the baseline as base though
    return "gpu_" + "_".join(flags) if flags else "gpu_base"

#all combination series
#GPU_COMBOS   = [(p, b, l) for p, b, l in product([False, True], repeat=3)]
#Ablation study series
#GPU_COMBOS = [(False, False, False), (True, False, False), (True, True, False), (True, True, True)]
#use for making limited plots
GPU_COMBOS = [(True, True, False), (True, True, True)]
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
#Simple Color Map
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

#RESOLUTIONS   = [(320, 180), (640, 360), (1280, 720), (1920, 1080)]
RESOLUTIONS = [(1920, 1080)]
RES_COLORS   = ["#1f77b4"]#, "#ff7f0e", "#2ca02c", "#9467bd"]
RES_COLOR_MAP = dict(zip(RESOLUTIONS, RES_COLORS))
SPHERE_COUNTS = [1, 5, 10, 25, 50, 100, 200, 500]

MAX_CPU_SPHERES = 25

# GPU_COMBOS = [
#     (p, b, l)
#     for p, b, l in product([False, True], repeat=3)
# ]


RUNS_PER_GPU_CONFIG  = 10
RUNS_PER_CPU_CONFIG = 3

BINARY = "./raytracer"
#NUM_DYNAMIC = 100
DYNAMIC_RATIO = .1
MAX_CPU_SPHERES = 25

LINE_WIDTH = 5
MARKER_SIZE = 10
TITLE_SIZE = 50
AXIS_TITLE_SIZE = 20
TICK_FONT_SIZE = 20
TICK_WIDTH = 2
TICK_LEN = 10
GENERAL_FONT_SIZE = 18
LEGEND_FONT_SIZE = 25
LEGEND_TITLE_SIZE = 30
SUBPLOT_TITLE_SIZE = 40