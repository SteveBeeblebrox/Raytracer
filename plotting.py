import pandas as pd
import plotly.graph_objects as go

df = pd.read_csv("results.csv")
df["resolution"] = df["width"].astype(str) + "x" + df["height"].astype(str)
df_avg = df.groupby(["num_spheres", "resolution"])["time_ms"].mean().reset_index()

resolutions = ["320x180", "640x360", "1280x720", "1920x1080"]
colors = ["#636EFA", "#EF553B", "#00CC96", "#AB63FA"]

fig = go.Figure()

for res, color in zip(resolutions, colors):
    subset = df_avg[df_avg["resolution"] == res].sort_values("num_spheres")
    fig.add_trace(go.Scatter(
        x=subset["num_spheres"],
        y=subset["time_ms"] / 1000,
        mode="lines+markers",
        name=res,
        line=dict(color=color, width=4),        # thicker line
        marker=dict(size=12),                   # bigger markers
        hovertemplate="<b>%{fullData.name}</b><br>Spheres: %{x}<br>Time: %{y:.2f}s<extra></extra>"
    ))

fig.update_layout(
    title=dict(
        text="CPU Raytracer Serial Execution Time",
        font=dict(size=24)                      # bigger title
    ),
    xaxis=dict(
        title="Number of Spheres",
        title_font=dict(size=18),               # bigger axis label
        tickfont=dict(size=15),                 # bigger tick numbers
        tickvals=df_avg["num_spheres"].unique(),
    ),
    yaxis=dict(
        title="Average Render Time (seconds)",
        title_font=dict(size=18),
        tickfont=dict(size=15),
    ),
    legend=dict(
        title=dict(text="Resolution", font=dict(size=16)),
        font=dict(size=14),                     # bigger legend labels
        borderwidth=1,
    ),
    hovermode="x unified",
    template="plotly_white",
    width=900,
    height=550,
)

fig.write_html("benchmark.html")
fig.show()