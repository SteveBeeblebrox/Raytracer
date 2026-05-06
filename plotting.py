#!/home/andromeda/Raytracer/venv/bin/python3
import pandas as pd
import plotly.graph_objects as go
from plotly.subplots import make_subplots

df = pd.read_csv("results.csv")
df["resolution"] = df["width"].astype(str) + "x" + df["height"].astype(str)
df_avg = df.groupby(["num_spheres", "resolution", "runtime"])["time_ms"].mean().reset_index()

resolutions = ["320x180", "640x360", "1280x720", "1920x1080"]
colors = ["#1f77b4", "#ff7f0e", "#2ca02c", "#9467bd"]
color_map = dict(zip(resolutions, colors))
dash_map = {"cpu": "dash", "gpu": "solid"}
name_map = {"cpu": "CPU", "gpu": "GPU"}

# compute speedup: cpu_time / gpu_time per (n_spheres, resolution)
cpu_df = df_avg[df_avg["runtime"] == "cpu"][["num_spheres", "resolution", "time_ms"]].rename(columns={"time_ms": "cpu_ms"})
gpu_df = df_avg[df_avg["runtime"] == "gpu"][["num_spheres", "resolution", "time_ms"]].rename(columns={"time_ms": "gpu_ms"})
speedup_df = pd.merge(cpu_df, gpu_df, on=["num_spheres", "resolution"])
speedup_df["speedup"] = speedup_df["cpu_ms"] / speedup_df["gpu_ms"]

fig = make_subplots(
    rows=2, cols=1,
    subplot_titles=("Render Time (log-log)", "GPU Speedup over CPU"),
    vertical_spacing=0.12,
    row_heights=[0.6, 0.4]
)

# ── top plot: log-log render times ───────────────────────────────────────────
for runtime in ["cpu", "gpu"]:
    for res in resolutions:
        subset = df_avg[
            (df_avg["resolution"] == res) &
            (df_avg["runtime"] == runtime)
        ].sort_values("num_spheres")

        if subset.empty:
            continue

        fig.add_trace(go.Scatter(
            x=subset["num_spheres"],
            y=subset["time_ms"] / 1000,
            mode="lines+markers",
            name=f"{name_map[runtime]} – {res}",
            line=dict(color=color_map[res], width=3, dash=dash_map[runtime]),
            marker=dict(size=9),
            legendgroup=res,
            hovertemplate=f"<b>{name_map[runtime]} – {res}</b><br>Spheres: %{{x}}<br>Time: %{{y:.3f}}s<extra></extra>"
        ), row=1, col=1)

# legend key for line styles
for label, dash in [("── GPU", "solid"), ("╌╌ CPU", "dash")]:
    fig.add_trace(go.Scatter(
        x=[None], y=[None], mode="lines",
        line=dict(color="grey", dash=dash, width=3),
        name=label, showlegend=True
    ), row=1, col=1)

# ── bottom plot: speedup ratio ────────────────────────────────────────────────
for res in resolutions:
    subset = speedup_df[speedup_df["resolution"] == res].sort_values("num_spheres")
    if subset.empty:
        continue

    fig.add_trace(go.Scatter(
        x=subset["num_spheres"],
        y=subset["speedup"],
        mode="lines+markers",
        name=res,
        line=dict(color=color_map[res], width=3),
        marker=dict(size=9),
        legendgroup=res,
        showlegend=False,
        hovertemplate=f"<b>{res}</b><br>Spheres: %{{x}}<br>Speedup: %{{y:.1f}}x<extra></extra>"
    ), row=2, col=1)

# reference line at speedup = 1
fig.add_hline(y=1, line_dash="dot", line_color="grey", line_width=1.5, row=2, col=1)

fig.update_xaxes(type="log", title_text="Number of Spheres", title_font=dict(size=16), tickfont=dict(size=13), row=1, col=1)
fig.update_xaxes(type="linear", title_text="Number of Spheres", title_font=dict(size=16), tickfont=dict(size=13), row=2, col=1)
fig.update_yaxes(type="log", title_text="Render Time (s)",   title_font=dict(size=16), tickfont=dict(size=13), row=1, col=1)
fig.update_yaxes(             title_text="Speedup (×)",      title_font=dict(size=16), tickfont=dict(size=13), row=2, col=1)

fig.update_layout(
    title=dict(text="CPU vs GPU Raytracer Performance", font=dict(size=24)),
    legend=dict(title=dict(text="Resolution / Runtime", font=dict(size=15)), font=dict(size=13), borderwidth=1),
    hovermode="x unified",
    template="plotly_white",
    width=1000,
    height=800,
)

fig.write_html("benchmark.html")
fig.show()