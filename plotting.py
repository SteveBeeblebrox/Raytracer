#!/home/andromeda/Raytracer/venv/bin/python3
import pandas as pd
import plotly.graph_objects as go
from plotly.subplots import make_subplots
from plot_config import GPU_RUNTIMES, GPU_LABELS, GPU_COLORS, RESOLUTIONS, RES_COLOR_MAP

df = pd.read_csv("results.csv")
df["resolution"] = df["width"].astype(str) + "x" + df["height"].astype(str)

# average across runs first, then across RESOLUTIONS for cleaner lines
df_avg = df.groupby(["num_spheres", "resolution", "runtime"])["time_ms"].mean().reset_index()
df_res_avg = df_avg.groupby(["num_spheres", "runtime"])["time_ms"].mean().reset_index()


sphere_counts = sorted(df["num_spheres"].unique())

# ═══════════════════════════════════════════════════════════════════════════════
# FIGURE 1 — CPU vs GPU (log-log + speedup), averaged across RESOLUTIONS
# ═══════════════════════════════════════════════════════════════════════════════
fig1 = make_subplots(
    rows=2, cols=1,
    subplot_titles=("Render Time — CPU vs GPU Variants (log-log)", "GPU Speedup over CPU"),
    vertical_spacing=0.12,
    row_heights=[0.6, 0.4]
)

# CPU line
cpu_data = df_res_avg[df_res_avg["runtime"] == "cpu"].sort_values("num_spheres")
fig1.add_trace(go.Scatter(
    x=cpu_data["num_spheres"], y=cpu_data["time_ms"] / 1000,
    mode="lines+markers", name="CPU",
    line=dict(color="black", width=3, dash="dash"), marker=dict(size=9),
    hovertemplate="<b>CPU</b><br>Spheres: %{x}<br>Time: %{y:.3f}s<extra></extra>"
), row=1, col=1)

# GPU variant lines + speedup
for rt in GPU_RUNTIMES:
    gpu_data = df_res_avg[df_res_avg["runtime"] == rt].sort_values("num_spheres")
    if gpu_data.empty:
        continue

    fig1.add_trace(go.Scatter(
        x=gpu_data["num_spheres"], y=gpu_data["time_ms"] / 1000,
        mode="lines+markers", name=GPU_LABELS[rt],
        line=dict(color=GPU_COLORS[rt], width=3), marker=dict(size=9),
        legendgroup=rt,
        hovertemplate=f"<b>{GPU_LABELS[rt]}</b><br>Spheres: %{{x}}<br>Time: %{{y:.3f}}s<extra></extra>"
    ), row=1, col=1)

    # speedup vs CPU (only where CPU data exists)
    merged = pd.merge(
        cpu_data[["num_spheres", "time_ms"]].rename(columns={"time_ms": "cpu_ms"}),
        gpu_data[["num_spheres", "time_ms"]].rename(columns={"time_ms": "gpu_ms"}),
        on="num_spheres"
    )
    merged["speedup"] = merged["cpu_ms"] / merged["gpu_ms"]

    fig1.add_trace(go.Scatter(
        x=merged["num_spheres"], y=merged["speedup"],
        mode="lines+markers", name=GPU_LABELS[rt],
        line=dict(color=GPU_COLORS[rt], width=3), marker=dict(size=9),
        legendgroup=rt, showlegend=False,
        hovertemplate=f"<b>{GPU_LABELS[rt]}</b><br>Spheres: %{{x}}<br>Speedup: %{{y:.1f}}x<extra></extra>"
    ), row=2, col=1)

fig1.add_hline(y=1, line_dash="dot", line_color="grey", line_width=1.5, row=2, col=1)

fig1.update_xaxes(type="log",    title_text="Number of Spheres", title_font=dict(size=16), tickfont=dict(size=13), row=1, col=1)
fig1.update_xaxes(type="linear", title_text="Number of Spheres", title_font=dict(size=16), tickfont=dict(size=13), tickvals=sphere_counts, row=2, col=1)
fig1.update_yaxes(type="log",    title_text="Render Time (s)",   title_font=dict(size=16), tickfont=dict(size=13), row=1, col=1)
fig1.update_yaxes(type="linear", title_text="Speedup (×)",       title_font=dict(size=16), tickfont=dict(size=13), row=2, col=1)

fig1.update_layout(
    title=dict(text="CPU vs GPU Raytracer Performance", font=dict(size=24)),
    legend=dict(title=dict(text="Runtime", font=dict(size=15)), font=dict(size=13), borderwidth=1),
    hovermode="x unified", template="plotly_white", width=1000, height=800,
)
fig1.write_html("benchmark_cpu_vs_gpu.html")
#fig1.write_image("benchmark_cpu_vs_gpu.png")

# ═══════════════════════════════════════════════════════════════════════════════
# FIGURE 2 — GPU variants only, broken out by resolution, linear scale
# ═══════════════════════════════════════════════════════════════════════════════

# Create a 2x2 grid (or adjust rows/cols based on your RESOLUTIONS length)
fig2 = make_subplots(
    rows=2, cols=2,
    subplot_titles=[f"Resolution: {r[0]}x{r[1]}" for r in RESOLUTIONS],
    shared_xaxes=True,
    vertical_spacing=0.15,
    horizontal_spacing=0.10
)

# Map resolutions to their grid positions
res_pos = {f"{r[0]}x{r[1]}": (i // 2 + 1, i % 2 + 1) for i, r in enumerate(RESOLUTIONS)}

for res_tuple in RESOLUTIONS:
    res_str = f"{res_tuple[0]}x{res_tuple[1]}"
    row, col = res_pos[res_str]
    
    for rt in GPU_RUNTIMES:
        subset = df_avg[
            (df_avg["runtime"] == rt) &
            (df_avg["resolution"] == res_str)
        ].sort_values("num_spheres")
        
        if subset.empty:
            continue

        # We only want one legend entry per Runtime, not per Subplot
        show_legend = (res_str == f"{RESOLUTIONS[0][0]}x{RESOLUTIONS[0][1]}")

        fig2.add_trace(go.Scatter(
            x=subset["num_spheres"], 
            y=subset["time_ms"] / 1000,
            mode="lines+markers", 
            name=GPU_LABELS.get(rt, rt),
            line=dict(color=GPU_COLORS.get(rt, "black"), width=3),
            marker=dict(size=8),
            legendgroup=rt, # Keeps toggle behavior synced across subplots
            showlegend=show_legend,
            hovertemplate=f"<b>{GPU_LABELS.get(rt, rt)}</b><br>Spheres: %{{x}}<br>Time: %{{y:.3f}}s<extra></extra>"
        ), row=row, col=col)

# Update axes titles
for i in range(1, 3):
    fig2.update_xaxes(title_text="Number of Spheres", row=2, col=i)
    fig2.update_yaxes(title_text="Render Time (s)", col=1, row=i)

fig2.update_layout(
    title=dict(text="GPU Performance by Optimization Strategy", font=dict(size=24)),
    legend=dict(title=dict(text="Optimization"), font=dict(size=13), borderwidth=1),
    template="plotly_white",
    width=1100,
    height=850,
    hovermode="x unified"
)
fig2.write_html("benchmark_gpu_variants.html")
#fig2.write_image("benchmark_gpu_variants.png")

print("Saved benchmark_cpu_vs_gpu.html and benchmark_gpu_variants.html")