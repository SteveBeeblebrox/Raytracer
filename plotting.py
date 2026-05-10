#!/home/andromeda/Raytracer/venv/bin/python3
import pandas as pd
import plotly.graph_objects as go
from plotly.subplots import make_subplots
from plot_config import GPU_RUNTIMES, GPU_LABELS, GPU_COLORS, RESOLUTIONS, RES_COLOR_MAP

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
            line=dict(color=ALL_COLORS[rt], width=2.5, dash=ALL_DASHES[rt]),
            marker=dict(size=7),
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
                      title_font=dict(size=13), tickfont=dict(size=11),
                      row=row, col=col)
# for row in range(0, 4):
        fig1.update_yaxes(type="log", title_text="Avg Render Time (s)",
                      title_font=dict(size=13), tickfont=dict(size=11),
                      row=row, col=col)

fig1.update_layout(
    title=dict(text="CPU vs GPU Render Time by Resolution (log-log)", font=dict(size=22)),
    legend=dict(title=dict(text="Runtime", font=dict(size=14)),
                font=dict(size=12), borderwidth=1),
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
            line=dict(color=GPU_COLORS[rt], width=2.5),
            marker=dict(size=7),
            legendgroup=rt,
            showlegend=show_legend,
            hovertemplate=f"<b>{GPU_LABELS[rt]}</b><br>"
                          f"Resolution: {res}<br>"
                          f"Spheres: %{{x}}<br>"
                          f"Speedup: %{{y:.2f}}×<extra></extra>"
        ), row=row, col=col)

    fig2.add_hline(y=1, line_dash="dot", line_color="grey",
                   line_width=1.5, row=row, col=col)

for col in range(1, 3):
    for row in range(1, 3):
        fig2.update_xaxes(title_text="Number of Spheres",
                      title_font=dict(size=13), tickfont=dict(size=11),
                      tickvals=sphere_counts, row=row, col=col)
        fig2.update_yaxes(title_text="Speedup over CPU (×)",
                      title_font=dict(size=13), tickfont=dict(size=11),
                      row=row, col=col)

fig2.update_layout(
    title=dict(text="GPU Speedup over CPU by Resolution", font=dict(size=22)),
    legend=dict(title=dict(text="GPU Variant", font=dict(size=14)),
                font=dict(size=12), borderwidth=1),
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
            line=dict(color=GPU_COLORS[rt], width=2.5),
            marker=dict(size=7),
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
                      title_font=dict(size=13), tickfont=dict(size=11),
                      tickvals=sphere_counts, row=row, col=col)
        fig3.update_yaxes(type = "log",title_text="Avg Render Time (s)",
                      title_font=dict(size=13), tickfont=dict(size=11),
                      row=row, col=col)

fig3.update_layout(
    title=dict(text="GPU Optimization Variants — Render Time by Resolution", font=dict(size=22)),
    legend=dict(title=dict(text="GPU Variant", font=dict(size=14)),
                font=dict(size=12), borderwidth=1),
    hovermode="x unified",
    template="plotly_white",
    width=1100, height=850,
)
fig3.write_html("benchmark_gpu_variants.html")

print("Saved benchmark_cpu_vs_gpu.html, benchmark_speedup.html, benchmark_gpu_variants.html")