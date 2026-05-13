import pandas as pd
import plotly.graph_objects as go
from plotly.subplots import make_subplots
from plot_config import GPU_RUNTIMES, GPU_LABELS, GPU_COLORS, RESOLUTIONS, RES_COLOR_MAP, LINE_WIDTH, MARKER_SIZE, TITLE_SIZE, AXIS_TITLE_SIZE, TICK_FONT_SIZE, TICK_WIDTH, TICK_LEN, GENERAL_FONT_SIZE, LEGEND_FONT_SIZE, LEGEND_TITLE_SIZE

df = pd.read_csv("results.csv")
df["resolution"] = df["width"].astype(str) + "x" + df["height"].astype(str)

df_avg     = df.groupby(["num_spheres", "resolution", "runtime"])["time_ms"].mean().reset_index()
df_res_avg = df_avg.groupby(["num_spheres", "runtime"])["time_ms"].mean().reset_index()

RES_LABELS    = [f"{w}x{h}" for (w, h) in RESOLUTIONS]
sphere_counts = sorted(df["num_spheres"].unique())
res_pos       = {res: (i // 2 + 1, i % 2 + 1) for i, res in enumerate(RES_LABELS)}

ALL_RUNTIMES  = ["cpu"] + GPU_RUNTIMES
ALL_LABELS    = {"cpu": "CPU", **GPU_LABELS}
ALL_COLORS    = {"cpu": "black", **GPU_COLORS}
ALL_DASHES    = {"cpu": "dash", **{rt: "solid" for rt in GPU_RUNTIMES}}

# ═══════════════════════════════════════════════════════════════════════════════
# FIGURE 1 — CPU + GPU render time, 2x2 subplots by resolution (log-log)
# ═══════════════════════════════════════════════════════════════════════════════
fig1 = make_subplots(
    rows=2, cols=2,
    subplot_titles=[f"Resolution: {r}" for r in RES_LABELS],
    #shared_xaxes=True,
    #shared_yaxes=True,
    vertical_spacing=0.12,
    horizontal_spacing=0.08,
)

for res in RES_LABELS:
    row, col = res_pos[res]
    for rt in ALL_RUNTIMES:
        subset = df_avg[
            (df_avg["runtime"]    == rt) &
            (df_avg["resolution"] == res)
        ].sort_values("num_spheres")

        if subset.empty:
            continue

        show_legend = (res == RES_LABELS[0])
        fig1.add_trace(go.Scatter(
            x=subset["num_spheres"],
            y=subset["time_ms"] / 1000,
            mode="lines+markers",
            name=ALL_LABELS.get(rt, rt),
            line=dict(color=ALL_COLORS[rt], width=LINE_WIDTH, dash=ALL_DASHES[rt]),
            marker=dict(size=MARKER_SIZE),
            legendgroup=rt,
            showlegend=show_legend,
            hovertemplate=f"<b>{ALL_LABELS.get(rt, rt)}</b><br>"
                          f"Resolution: {res}<br>"
                          f"Spheres: %{{x}}<br>"
                          f"Avg Render Time: %{{y:.3f}}s<extra></extra>"
        ), row=row, col=col)

# axis labels on outer edges only
for col in range(0, 4):
    for row in range(0, 4):
        fig1.update_xaxes(type="log", title_text="Number of Spheres",
                      title_font=dict(size=AXIS_TITLE_SIZE), tickfont=dict(size=TICK_FONT_SIZE), tickwidth=TICK_WIDTH, ticklen=TICK_LEN,
                      row=row, col=col)
# for row in range(0, 4):
        fig1.update_yaxes(type="log", title_text="Avg Render Time (s)",
                      title_font=dict(size=AXIS_TITLE_SIZE), tickfont=dict(size=TICK_FONT_SIZE), tickwidth=TICK_WIDTH, ticklen=TICK_LEN,
                      row=row, col=col)

fig1.update_layout(
    title=dict(text="CPU vs GPU Render Time by Resolution (log-log)", font=dict(size=TITLE_SIZE)),
    legend=dict(title=dict(text="Runtime", font=dict(size=LEGEND_TITLE_SIZE)),
                font=dict(size=LEGEND_FONT_SIZE), borderwidth=1),
    hovermode="x unified",
    template="plotly_white",
    width=1100, height=850,
)

# after all traces are added, before update_layout
fig1.update_xaxes(matches="x")

fig1.write_html("benchmark_cpu_vs_gpu.html")

# ═══════════════════════════════════════════════════════════════════════════════
# FIGURE 2 — Speedup over CPU, 2x2 subplots by resolution
# ═══════════════════════════════════════════════════════════════════════════════
cpu_avg = df_avg[df_avg["runtime"] == "cpu"][["num_spheres", "resolution", "time_ms"]]\
            .rename(columns={"time_ms": "cpu_ms"})

fig2 = make_subplots(
    rows=2, cols=2,
    subplot_titles=[f"Resolution: {r}" for r in RES_LABELS],
    #shared_xaxes=True,
    #shared_yaxes=True,
    vertical_spacing=0.12,
    horizontal_spacing=0.08,
)

for res in RES_LABELS:
    row, col = res_pos[res]
    cpu_res = cpu_avg[cpu_avg["resolution"] == res]

    for rt in GPU_RUNTIMES:
        gpu_res = df_avg[
            (df_avg["runtime"]    == rt) &
            (df_avg["resolution"] == res)
        ][["num_spheres", "time_ms"]].rename(columns={"time_ms": "gpu_ms"})

        if gpu_res.empty:
            continue

        merged = pd.merge(cpu_res[["num_spheres", "cpu_ms"]], gpu_res, on="num_spheres")
        merged["speedup"] = merged["cpu_ms"] / merged["gpu_ms"]

        show_legend = (res == RES_LABELS[0])
        fig2.add_trace(go.Scatter(
            x=merged["num_spheres"],
            y=merged["speedup"],
            mode="lines+markers",
            name=GPU_LABELS[rt],
            line=dict(color=GPU_COLORS[rt], width=LINE_WIDTH),
            marker=dict(size=MARKER_SIZE),
            legendgroup=rt,
            showlegend=show_legend,
            hovertemplate=f"<b>{GPU_LABELS[rt]}</b><br>"
                          f"Resolution: {res}<br>"
                          f"Spheres: %{{x}}<br>"
                          f"Speedup: %{{y:.2f}}×<extra></extra>"
        ), row=row, col=col)

    fig2.add_hline(y=1, line_dash="dot", line_color="grey",
                   line_width=LINE_WIDTH, row=row, col=col)

for col in range(1, 3):
    for row in range(1, 3):
        fig2.update_xaxes(title_text="Number of Spheres",
                      title_font=dict(size=AXIS_TITLE_SIZE), tickfont=dict(size=TICK_FONT_SIZE), tickwidth=TICK_WIDTH, ticklen=TICK_LEN,
                      tickvals=sphere_counts, row=row, col=col)
        fig2.update_yaxes(title_text="Speedup over CPU (×)",
                      title_font=dict(size=AXIS_TITLE_SIZE), tickfont=dict(size=TICK_FONT_SIZE), tickwidth=TICK_WIDTH, ticklen=TICK_LEN,
                      row=row, col=col)

fig2.update_layout(
    title=dict(text="GPU Speedup over CPU by Resolution", font=dict(size=TITLE_SIZE)),
    legend=dict(title=dict(text="GPU Variant", font=dict(size=LEGEND_TITLE_SIZE)),
                font=dict(size=LEGEND_FONT_SIZE), borderwidth=1),
    hovermode="x unified",
    template="plotly_white",
    width=1100, height=850,
)
fig2.write_html("benchmark_speedup.html")

# ═══════════════════════════════════════════════════════════════════════════════
# FIGURE 3 — GPU variants only, 2x2 subplots by resolution, linear scale
# ═══════════════════════════════════════════════════════════════════════════════
fig3 = make_subplots(
    rows=2, cols=2,
    subplot_titles=[f"Resolution: {r}" for r in RES_LABELS],
    #shared_xaxes=True,
    #shared_yaxes=True,
    vertical_spacing=0.12,
    horizontal_spacing=0.08,
)

for res in RES_LABELS:
    row, col = res_pos[res]
    for rt in GPU_RUNTIMES:
        subset = df_avg[
            (df_avg["runtime"]    == rt) &
            (df_avg["resolution"] == res)
        ].sort_values("num_spheres")

        if subset.empty:
            continue

        show_legend = (res == RES_LABELS[0])
        fig3.add_trace(go.Scatter(
            x=subset["num_spheres"],
            y=subset["time_ms"] / 1000,
            mode="lines+markers",
            name=GPU_LABELS[rt],
            line=dict(color=GPU_COLORS[rt], width=LINE_WIDTH),
            marker=dict(size=MARKER_SIZE),
            legendgroup=rt,
            showlegend=show_legend,
            hovertemplate=f"<b>{GPU_LABELS[rt]}</b><br>"
                          f"Resolution: {res}<br>"
                          f"Spheres: %{{x}}<br>"
                          f"Avg Render Time: %{{y:.3f}}s<extra></extra>"
        ), row=row, col=col)

for col in range(1, 3):
    for row in range(1, 3):
        fig3.update_xaxes(type = "log", title_text="Number of Spheres",
                      title_font=dict(size=AXIS_TITLE_SIZE), tickfont=dict(size=TICK_FONT_SIZE), tickwidth=TICK_WIDTH, ticklen=TICK_LEN,
                      tickvals=sphere_counts, row=row, col=col)
        fig3.update_yaxes(type = "log",title_text="Avg Render Time (s)",
                      title_font=dict(size=AXIS_TITLE_SIZE), tickfont=dict(size=TICK_FONT_SIZE), tickwidth=TICK_WIDTH, ticklen=TICK_LEN,
                      row=row, col=col)

fig3.update_layout(
    title=dict(text="GPU Optimization Variants — Render Time by Resolution", font=dict(size=TITLE_SIZE)),
    legend=dict(title=dict(text="GPU Variant", font=dict(size=LEGEND_TITLE_SIZE)),
                font=dict(size=LEGEND_FONT_SIZE), borderwidth=1),
    hovermode="x unified",
    template="plotly_white",
    width=1100, height=850,
)
fig3.write_html("benchmark_gpu_variants.html")

# ═══════════════════════════════════════════════════════════════════════════════
# FIGURE 4 — Render time vs pixel area at fixed sphere count
# ═══════════════════════════════════════════════════════════════════════════════
FIXED_SPHERES = 500

df["pixels"] = df["width"] * df["height"]
df_fixed = df[df["num_spheres"] == FIXED_SPHERES].copy()
df_fixed_avg = df_fixed.groupby(["pixels", "runtime"])["time_ms"].mean().reset_index()

pixel_labels = {w*h: f"{w}x{h}" for (w, h) in RESOLUTIONS}
pixel_vals   = sorted(df_fixed_avg["pixels"].unique())

fig4 = go.Figure()

for rt in ALL_RUNTIMES:
    subset = df_fixed_avg[df_fixed_avg["runtime"] == rt].sort_values("pixels")
    if subset.empty:
        continue
    fig4.add_trace(go.Scatter(
        x=subset["pixels"],
        y=subset["time_ms"] / 1000,
        mode="lines+markers",
        name=ALL_LABELS.get(rt, rt),
        showlegend=True,
        line=dict(color=ALL_COLORS[rt], width=LINE_WIDTH, dash=ALL_DASHES[rt]),
        marker=dict(size=MARKER_SIZE),
        hovertemplate=f"<b>{ALL_LABELS.get(rt, rt)}</b><br>"
                      f"Resolution: %{{customdata}}<br>"
                      f"Pixels: %{{x:,}}<br>"
                      f"Avg Render Time: %{{y:.3f}}s<extra></extra>",
        customdata=[pixel_labels.get(p, str(p)) for p in subset["pixels"]],
    ))

fig4.update_xaxes(
    title_text="Pixel Area (width × height)",
    title_font=dict(size=AXIS_TITLE_SIZE), tickfont=dict(size=TICK_FONT_SIZE), tickwidth=TICK_WIDTH, ticklen=TICK_LEN,
    tickvals=pixel_vals,
    ticktext=[pixel_labels.get(p, str(p)) for p in pixel_vals],
)
fig4.update_yaxes(
    title_text="Avg Render Time (s)",
    title_font=dict(size=AXIS_TITLE_SIZE), tickfont=dict(size=TICK_FONT_SIZE), tickwidth=TICK_WIDTH, ticklen=TICK_LEN,
)

fig4.update_layout(
    title=dict(text=f"Render Time vs Resolution at {FIXED_SPHERES} Spheres", font=dict(size=TITLE_SIZE)),
    legend=dict(title=dict(text="Runtime", font=dict(size=LEGEND_TITLE_SIZE)),
                font=dict(size=LEGEND_FONT_SIZE), borderwidth=1),
    hovermode="x unified",
    template="plotly_white",
    width=900, height=550,
)

fig4.write_html("benchmark_by_resolution.html")


# ═══════════════════════════════════════════════════════════════════════════════
# FIGURE 5 — CPU performance: two panel side-by-side
#   Left:  render time vs sphere count at fixed resolution (1920x1080)
#   Right: render time vs pixel area at fixed sphere count (max CPU spheres)
# ═══════════════════════════════════════════════════════════════════════════════
CPU_FIXED_RES    = "1920x1080"
CPU_FIXED_SPHERES = 25   # max CPU sphere count

cpu_only = df_avg[df_avg["runtime"] == "cpu"].copy()
cpu_only["pixels"] = cpu_only["resolution"].map(
    {f"{w}x{h}": w*h for (w, h) in RESOLUTIONS}
)

pixel_labels = {w*h: f"{w}x{h}" for (w, h) in RESOLUTIONS}
pixel_vals   = sorted([w*h for (w, h) in RESOLUTIONS])

fig5 = make_subplots(
    rows=1, cols=2,
    subplot_titles=(
        f"Render Time vs Scene Complexity ({CPU_FIXED_RES})",
        f"Render Time vs Resolution ({CPU_FIXED_SPHERES} Spheres)"
    ),
    horizontal_spacing=0.12,
)

# Left — sphere count sweep at fixed resolution
left_data = cpu_only[cpu_only["resolution"] == CPU_FIXED_RES].sort_values("num_spheres")
fig5.add_trace(go.Scatter(
    x=left_data["num_spheres"],
    y=left_data["time_ms"] / 1000,
    mode="lines+markers",
    name=CPU_FIXED_RES,
    line=dict(color="black", width=LINE_WIDTH),
    marker=dict(size=MARKER_SIZE),
    showlegend=False,
    hovertemplate="Spheres: %{x}<br>Avg Render Time: %{y:.3f}s<extra></extra>"
), row=1, col=1)

# Right — resolution sweep at fixed sphere count
right_data = cpu_only[cpu_only["num_spheres"] == CPU_FIXED_SPHERES].sort_values("pixels")
fig5.add_trace(go.Scatter(
    x=right_data["pixels"],
    y=right_data["time_ms"] / 1000,
    mode="lines+markers",
    name=str(CPU_FIXED_SPHERES),
    line=dict(color="black", width=LINE_WIDTH),
    marker=dict(size=MARKER_SIZE),
    showlegend=False,
    hovertemplate="Resolution: %{customdata}<br>Avg Render Time: %{y:.3f}s<extra></extra>",
    customdata=[pixel_labels.get(p, str(p)) for p in right_data["pixels"]],
), row=1, col=2)

fig5.update_xaxes(
    title_text="Number of Spheres",
    title_font=dict(size=AXIS_TITLE_SIZE), tickfont=dict(size=TICK_FONT_SIZE), tickwidth=TICK_WIDTH, ticklen=TICK_LEN,
    tickvals=sorted(left_data["num_spheres"].unique()),
    row=1, col=1
)
fig5.update_xaxes(
    title_text="Pixel Area (width × height)",
    title_font=dict(size=AXIS_TITLE_SIZE), tickfont=dict(size=TICK_FONT_SIZE), tickwidth=TICK_WIDTH, ticklen=TICK_LEN,
    tickvals=pixel_vals,
    ticktext=[pixel_labels[p] for p in pixel_vals],
    row=1, col=2
)
fig5.update_yaxes(
    title_text="Avg Render Time (s)",
    title_font=dict(size=AXIS_TITLE_SIZE), tickfont=dict(size=TICK_FONT_SIZE), tickwidth=TICK_WIDTH, ticklen=TICK_LEN,
    row=1, col=1
)
fig5.update_yaxes(
    title_text="Avg Render Time (s)",
    title_font=dict(size=AXIS_TITLE_SIZE), tickfont=dict(size=TICK_FONT_SIZE), tickwidth=TICK_WIDTH, ticklen=TICK_LEN,
    row=1, col=2
)

fig5.update_layout(
    title=dict(text="CPU Raytracer Performance", font=dict(size=TITLE_SIZE)),
    template="plotly_white",
    hovermode="x unified",
    width=1100, height=480,
)

fig5.write_html("benchmark_cpu.html")

# ═══════════════════════════════════════════════════════════════════════════════
# FIGURE 9 FIX — Speedup over CPU vs resolution at fixed sphere count
# ═══════════════════════════════════════════════════════════════════════════════
FIXED_SPHERES_SPEEDUP = 25
TARGET_RT = "gpu_partition_buffer"

pixel_labels = {w*h: f"{w}x{h}" for (w, h) in RESOLUTIONS}
pixel_vals   = sorted([w*h for (w, h) in RESOLUTIONS])

# get CPU baseline per resolution at fixed sphere count
cpu_base = df_avg[
    (df_avg["runtime"]     == "cpu") &
    (df_avg["num_spheres"] == FIXED_SPHERES_SPEEDUP)
][["resolution", "time_ms"]].rename(columns={"time_ms": "cpu_ms"})

# get target GPU variant at same sphere count
gpu_data = df_avg[
    (df_avg["runtime"]     == TARGET_RT) &
    (df_avg["num_spheres"] == FIXED_SPHERES_SPEEDUP)
][["resolution", "time_ms"]].rename(columns={"time_ms": "gpu_ms"})

merged = pd.merge(cpu_base, gpu_data, on="resolution")
merged["speedup"] = merged["cpu_ms"] / merged["gpu_ms"]
merged["pixels"]  = merged["resolution"].map({f"{w}x{h}": w*h for (w, h) in RESOLUTIONS})
merged = merged.sort_values("pixels")

fig_fix = go.Figure()
fig_fix.add_trace(go.Scatter(
    x=merged["pixels"],
    y=merged["speedup"],
    mode="lines+markers",
    name=GPU_LABELS[TARGET_RT],
    line=dict(color=GPU_COLORS[TARGET_RT], width=3),
    marker=dict(size=10),
    showlegend=True,
    hovertemplate="Resolution: %{customdata}<br>Speedup: %{y:.1f}×<extra></extra>",
    customdata=[pixel_labels[p] for p in merged["pixels"]],
))

fig_fix.update_xaxes(
    title_text="Pixel Area (width × height)",
    title_font=dict(size=15), tickfont=dict(size=13),
    tickvals=pixel_vals,
    ticktext=[pixel_labels[p] for p in pixel_vals],
)
fig_fix.update_yaxes(
    title_text="Speedup over CPU (×)",
    title_font=dict(size=15), tickfont=dict(size=13),
)
fig_fix.update_layout(
    title=dict(
        text=f"Speedup over CPU vs Resolution ({FIXED_SPHERES_SPEEDUP} Spheres)",
        font=dict(size=20)
    ),
    legend=dict(title=dict(text="GPU Variant", font=dict(size=13)), font=dict(size=12), borderwidth=1),
    template="plotly_white",
    hovermode="x unified",
    width=750, height=480,
)

fig_fix.write_html("benchmark_speedup_vs_resolution.html")

print("Saved benchmark_cpu_vs_gpu.html, benchmark_speedup.html, benchmark_gpu_variants.html, benchmark_by_resolution.html, benchmark_cpu.html benchmark_speedup_vs_resolution.html")