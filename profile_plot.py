#!/home/andromeda/Raytracer/venv/bin/python3
import pandas as pd
import plotly.graph_objects as go
from plotly.subplots import make_subplots
from plot_config import GPU_RUNTIMES, GPU_LABELS, GPU_COLORS, RESOLUTIONS

df = pd.read_csv("profile_results.csv")
df["res_str"] = df["width"].astype(str) + "x" + df["height"].astype(str)

RES_LABELS    = [f"{w}x{h}" for (w, h) in RESOLUTIONS]
sphere_counts = sorted(df["num_spheres"].unique())
res_pos       = {res: (i // 2 + 1, i % 2 + 1) for i, res in enumerate(RES_LABELS)}

# ── metric group definition ────────────────────────────────────────────────────
# Each entry:
#   columns      : list of CSV column names to plot (one line per column per runtime)
#   y_label      : y-axis label with units
#   y_desc       : longer description for the subplot annotation
#   series_labels: short label appended to runtime name for multi-column groups
#   title        : figure title
#   filename     : output HTML filename
# ─────────────────────────────────────────────────────────────────────────────
METRIC_GROUPS = [
    {
        "columns":       ["kernel_time_ms"],
        "y_label":       "Kernel Time (ms)",
        # "y_desc":        "Wall-clock time the GPU kernel ran, in milliseconds.\n"
        #                  "Measured per render call, averaged across NCU replay passes.",
        "series_labels": [""],
        "title":         "GPU Kernel Execution Time",
        "filename":      "profile_time.html",
    },
    {
        "columns":       ["sm__warps_active.avg.pct_of_peak_sustained_active"],
        "y_label":       "Achieved Occupancy (% of peak)",
        # "y_desc":        "Average fraction of the maximum possible active warps that were "
        #                  "active each cycle.\n100% = all warp slots filled. "
        #                  "Low values indicate the kernel is latency-bound or register-limited.",
        "series_labels": [""],
        "title":         "SM Warp Occupancy",
        "filename":      "profile_occupancy.html",
    },
    {
        "columns":       ["dram__throughput.avg.pct_of_peak_sustained_elapsed"],
        "y_label":       "DRAM Throughput (% of peak)",
        # "y_desc":        "Average DRAM bandwidth utilisation as a percentage of the GPU's "
        #                  "theoretical peak memory bandwidth.\n"
        #                  "High values mean the kernel is memory-bandwidth bound.",
        "series_labels": [""],
        "title":         "DRAM Bandwidth Utilisation",
        "filename":      "profile_dram.html",
    },
    {
        "columns":       ["lts__t_sector_hit_rate.pct",
                          "l1tex__t_sector_hit_rate.pct"],
        "y_label":       "Cache Hit Rate (%)",
        # "y_desc":        "Fraction of memory requests served from cache rather than DRAM.\n"
        #                  "L2 (LTS) covers requests from all SMs. "
        #                  "L1 (L1TEX) covers per-SM texture/global cache. "
        #                  "Higher = fewer expensive DRAM accesses.",
        "series_labels": ["L2", "L1"],
        "title":         "L1 / L2 Cache Hit Rates",
        "filename":      "profile_cache.html",
    },
    {
        "columns":       ["smsp__warps_issue_stalled_long_scoreboard.avg.pct_of_peak_sustained_active",
                          "smsp__warps_issue_stalled_math_pipe_throttle.avg.pct_of_peak_sustained_active"],
        "y_label":       "Stall Cycles (% of peak active cycles)",
        # "y_desc":        "Percentage of cycles where warps were stalled and could not issue.\n"
        #                  "'Memory stall' = waiting on a long-latency memory op (DRAM fetch).\n"
        #                  "'Math stall'   = compute pipeline was full / throttled.\n"
        #                  "High memory stall + low math stall → memory bound.\n"
        #                  "High math stall + low memory stall → compute bound.",
        "series_labels": ["Memory Stall", "Math Stall"],
        "title":         "Warp Stall Breakdown — Memory vs Compute",
        "filename":      "profile_stalls.html",
    },
    {
        "columns":       ["sm__inst_executed_pipe_fp32.avg.pct_of_peak_sustained_active"],
        "y_label":       "FP32 Pipe Utilisation (% of peak)",
        # "y_desc":        "Average utilisation of the FP32 (single-precision float) execution "
        #                  "pipelines as a fraction of peak throughput.\n"
        #                  "Raytracing is mostly FP32 arithmetic; low values here alongside "
        #                  "high memory stalls confirm the kernel is memory-bound.",
        "series_labels": [""],
        "title":         "FP32 Arithmetic Pipe Utilisation",
        "filename":      "profile_fp32.html",
    },
]

dash_cycle = ["solid", "dash", "dot", "dashdot"]

for mg in METRIC_GROUPS:
    columns       = mg["columns"]
    y_label       = mg["y_label"]
    #y_desc        = mg["y_desc"]
    series_labels = mg["series_labels"]
    title         = mg["title"]
    filename      = mg["filename"]

    fig = make_subplots(
        rows=2, cols=2,
        subplot_titles=[f"Resolution: {r}" for r in RES_LABELS],
        #shared_xaxes=True,
        #shared_yaxes=True,
        vertical_spacing=0.14,
        horizontal_spacing=0.08,
    )

    for rt in GPU_RUNTIMES:
        if rt not in df["runtime"].unique():
            continue

        for res in RES_LABELS:
            row, col = res_pos[res]
            subset = df[
                (df["runtime"] == rt) &
                (df["res_str"] == res)
            ].sort_values("num_spheres")

            if subset.empty:
                continue

            show_legend = (res == RES_LABELS[0])

            for mi, (metric, slabel) in enumerate(zip(columns, series_labels)):
                if metric not in df.columns:
                    continue

                full_label = GPU_LABELS.get(rt, rt)
                if slabel:
                    full_label += f" ({slabel})"

                fig.add_trace(go.Scatter(
                    x=subset["num_spheres"],
                    y=subset[metric],
                    mode="lines+markers",
                    name=full_label,
                    line=dict(color=GPU_COLORS.get(rt, "black"),
                              width=2.5,
                              dash=dash_cycle[mi]),
                    marker=dict(size=6),
                    legendgroup=f"{rt}_{mi}",
                    showlegend=show_legend,
                    hovertemplate=(
                        f"<b>{full_label}</b><br>"
                        f"Resolution: {res}<br>"
                        f"Spheres: %{{x}}<br>"
                        f"{y_label}: %{{y:.2f}}<extra></extra>"
                    )
                ), row=row, col=col)

    # x-axis labels on bottom row only
    for c in range(1, 3):
        for r in range(1, 3):
            fig.update_xaxes(type="log",
                title_text="Number of Spheres (scene complexity)",
                title_font=dict(size=12), tickfont=dict(size=10),
                tickvals=sphere_counts,
                row=r, col=c
            )

            fig.update_yaxes(type = "log",
                title_text=y_label,
                title_font=dict(size=12), tickfont=dict(size=10),
                row=r, col=c
            )

    fig.update_layout(
        title=dict(
            text=f"{title}<br>",
                 #f"<sup style='font-size:12px;color:grey'>{y_desc}</sup>",
            font=dict(size=20)
        ),
        legend=dict(
            title=dict(text="GPU Variant", font=dict(size=13)),
            font=dict(size=11),
            borderwidth=1
        ),
        hovermode="x unified",
        template="plotly_white",
        width=1100,
        height=900,
    )

    fig.write_html(filename)
    print(f"Saved {filename}")