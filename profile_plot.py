#!/home/andromeda/Raytracer/venv/bin/python3
import pandas as pd
import plotly.graph_objects as go
from plotly.subplots import make_subplots
from plot_config import GPU_RUNTIMES, GPU_LABELS, GPU_COLORS, RESOLUTIONS, LINE_WIDTH, MARKER_SIZE, TITLE_SIZE, AXIS_TITLE_SIZE, TICK_FONT_SIZE, TICK_WIDTH, TICK_LEN, GENERAL_FONT_SIZE, LEGEND_FONT_SIZE, LEGEND_TITLE_SIZE, SUBPLOT_TITLE_SIZE

df = pd.read_csv("profile_results.csv")
df["res_str"] = df["width"].astype(str) + "x" + df["height"].astype(str)

RES_LABELS    = [f"{w}x{h}" for (w, h) in RESOLUTIONS]
sphere_counts = sorted(df["num_spheres"].unique())
res_pos       = {res: (i // 2 + 1, i % 2 + 1) for i, res in enumerate(RES_LABELS)}

#columns lists the metrics to use
# y label is how it should appear on the plot
# series labels is for labeling the lines in the legend
# title is the title :O
# file name is the output file name :O

METRIC_GROUPS = [
    {
        "columns":       ["kernel_time_ms"],
        "y_label":       "Kernel Time (ms)",
        "series_labels": [""],
        "title":         "GPU Kernel Execution Time",
        "filename":      "profile_time.html",
    },
    {
        "columns":       ["sm__warps_active.avg.pct_of_peak_sustained_active"],
        "y_label":       "Achieved Occupancy (% of peak)",
        "series_labels": [""],
        "title":         "SM Warp Occupancy",
        "filename":      "profile_occupancy.html",
    },
    {
        "columns":       ["dram__throughput.avg.pct_of_peak_sustained_elapsed"],
        "y_label":       "DRAM Throughput (% of peak)",
        "series_labels": [""],
        "title":         "DRAM Bandwidth Utilisation",
        "filename":      "profile_dram.html",
    },
    {
        "columns":       ["lts__t_sector_hit_rate.pct",
                          "l1tex__t_sector_hit_rate.pct"],
        "y_label":       "Cache Hit Rate (%)",
        "series_labels": ["L2", "L1"],
        "title":         "L1 / L2 Cache Hit Rates",
        "filename":      "profile_cache.html",
    },
    {
        "columns":       ["smsp__warps_issue_stalled_long_scoreboard.avg.pct_of_peak_sustained_active",
                          "smsp__warps_issue_stalled_math_pipe_throttle.avg.pct_of_peak_sustained_active"],
        "y_label":       "Stall Cycles (% of peak active cycles)",
        "series_labels": ["Memory Stall", "Math Stall"],
        "title":         "Warp Stall Breakdown — Memory vs Compute",
        "filename":      "profile_stalls.html",
    },
    {
    "columns":       ["smsp__thread_inst_executed_per_inst_executed.avg",
                      "smsp__sass_average_branch_targets_threads_uniform.avg"],
    "y_label":       "Warp Efficiency",
    "series_labels": ["Active Threads/Warp (max 32)", "Uniform Branch Rate (0-1)"],
    "title":         "Warp Divergence Indicators",
    "filename":      "profile_divergence.html",
    },
]

#Plot it all in plotly
#plotly is harder to use but I love the interactivity when its time to actually look at the graphs

dash_cycle = ["solid", "dash", "dot", "dashdot"]

for mg in METRIC_GROUPS:
    columns       = mg["columns"]
    y_label       = mg["y_label"]
    #y_desc        = mg["y_desc"]
    series_labels = mg["series_labels"]
    title         = mg["title"]
    filename      = mg["filename"]

    fig = make_subplots(
        rows=1, cols=1,
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
                              width=LINE_WIDTH,
                              dash=dash_cycle[mi]),
                    marker=dict(size=MARKER_SIZE),
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
                title_font=dict(size=AXIS_TITLE_SIZE), tickfont=dict(size=TICK_FONT_SIZE), tickwidth=TICK_WIDTH, ticklen=TICK_LEN,
                tickvals=sphere_counts,
                row=r, col=c
            )

            fig.update_yaxes(type = "log",
                title_text=y_label,
                title_font=dict(size=AXIS_TITLE_SIZE), tickfont=dict(size=TICK_FONT_SIZE), tickwidth=TICK_WIDTH, ticklen=TICK_LEN,
                row=r, col=c
            )

    fig.update_layout(
        title=dict(
            text=f"{title}<br>",
            font=dict(size=TITLE_SIZE)
        ),
        legend=dict(
            title=dict(text="GPU Variant", font=dict(size=LEGEND_TITLE_SIZE)),
            font=dict(size=LEGEND_FONT_SIZE),
            borderwidth=1
        ),
        hovermode="x unified",
        template="plotly_white",
        width=1100,
        height=900,
    )

    fig.update_annotations(font_size=SUBPLOT_TITLE_SIZE)

    fig.write_html(filename)
    print(f"Saved {filename}")