#!/home/andromeda/Raytracer/venv/bin/python3
import pandas as pd
import plotly.graph_objects as go
from plotly.subplots import make_subplots
from plot_config import GPU_RUNTIMES, GPU_LABELS, GPU_COLORS, RESOLUTIONS

df = pd.read_csv("profile_results.csv")

# Ensure resolution matches the string format used in the titles
df["res_str"] = df["width"].astype(str) + "x" + df["height"].astype(str)

sphere_counts = sorted(df["num_spheres"].unique())

# Correct Metric Keys from your NCU output
METRIC_GROUPS = [
    (["kernel_time_ms"], "Time (ms)", "Kernel Execution Time", "profile_time.html"),
    (["sm__warps_active.avg.pct_of_peak_sustained_active"], "Occupancy (%)", "Achieved Occupancy", "profile_occupancy.html"),
    (["dram__throughput.avg.pct_of_peak_sustained_elapsed"], "% of Peak", "DRAM Throughput", "profile_dram.html"),
    (["lts__t_sector_hit_rate.pct", "l1tex__t_sector_hit_rate.pct"], "Hit Rate (%)", "L1/L2 Cache Hit Rates", "profile_cache.html"),
]

# Map the tuple RESOLUTIONS to the string labels used in subplots
RES_LABELS = [f"{w}x{h}" for (w, h) in RESOLUTIONS]

for (metrics, y_label, title, filename) in METRIC_GROUPS:
    fig = make_subplots(
        rows=2, cols=2,
        subplot_titles=RES_LABELS, # Use the string list here
        shared_xaxes=True,
        shared_yaxes=True,
        vertical_spacing=0.12,
        horizontal_spacing=0.08,
    )

    # Dictionary to map resolution string to grid position
    pos = {res_str: (i // 2 + 1, i % 2 + 1) for i, res_str in enumerate(RES_LABELS)}

    for rt in GPU_RUNTIMES:
        # Some runtimes might be missing from the CSV if the script was interrupted
        if rt not in df["runtime"].unique():
            continue
            
        for res_str in RES_LABELS:
            row, col = pos[res_str]
            
            # Filter using the helper string column
            subset = df[
                (df["runtime"] == rt) & 
                (df["res_str"] == res_str)
            ].sort_values("num_spheres")

            if subset.empty:
                continue

            for mi, metric in enumerate(metrics):
                show_legend = (res_str == RES_LABELS[0])
                
                # Clean up labels for the legend
                metric_name = "L2" if "lts" in metric else "L1" if "l1tex" in metric else ""
                label = f"{GPU_LABELS.get(rt, rt)}"
                if metric_name:
                    label += f" ({metric_name})"

                fig.add_trace(go.Scatter(
                    x=subset["num_spheres"],
                    y=subset[metric],
                    mode="lines+markers",
                    name=label,
                    line=dict(color=GPU_COLORS.get(rt, "black"), width=2, dash="dash" if mi > 0 else "solid"),
                    marker=dict(size=6),
                    legendgroup=f"{rt}_{mi}",
                    showlegend=show_legend,
                ), row=row, col=col)

    fig.update_layout(
        title=dict(text=title, font=dict(size=20)),
        template="plotly_white",
        width=1000, height=800,
        hovermode="x unified"
    )
    
    fig.write_html(filename)
    print(f"Successfully generated {filename}")