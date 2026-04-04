#!/usr/bin/env python3
import argparse
import json
from pathlib import Path
from xml.sax.saxutils import escape


TIME_KEYS = ("time_ms", "time", "timestamp_ms", "t_ms")
DEPTH_KEYS = ("depth_m", "depth", "depthMeter")
LIST_KEYS = ("samples", "data", "records", "items")


def pick_number(record, keys):
    for key in keys:
        value = record.get(key)
        if value is None:
            continue
        return float(value)
    raise KeyError(f"missing keys: {keys}")


def normalize_record(record):
    return {
        "idx": record.get("idx", record.get("index")),
        "time_ms": pick_number(record, TIME_KEYS),
        "depth_m": pick_number(record, DEPTH_KEYS),
    }


def parse_json_payload(text):
    payload = json.loads(text)

    if isinstance(payload, list):
        return [normalize_record(item) for item in payload if isinstance(item, dict)]

    if isinstance(payload, dict):
        for key in LIST_KEYS:
            value = payload.get(key)
            if isinstance(value, list):
                return [normalize_record(item) for item in value if isinstance(item, dict)]
        return [normalize_record(payload)]

    raise ValueError("unsupported JSON structure")


def parse_json_lines(text):
    samples = []
    for line_no, raw_line in enumerate(text.splitlines(), start=1):
        line = raw_line.strip()
        if not line:
            continue
        try:
            item = json.loads(line)
        except json.JSONDecodeError as exc:
            raise ValueError(f"invalid JSON on line {line_no}: {exc}") from exc
        if isinstance(item, dict):
            samples.append(normalize_record(item))
    return samples


def load_samples(path):
    text = path.read_text(encoding="utf-8").strip()
    if not text:
        raise ValueError("input file is empty")

    try:
        samples = parse_json_payload(text)
    except json.JSONDecodeError:
        samples = parse_json_lines(text)

    if not samples:
        raise ValueError("no valid samples found")

    samples.sort(key=lambda item: item["time_ms"])
    return samples


def format_label(value):
    text = f"{value:.2f}"
    text = text.rstrip("0").rstrip(".")
    return text if text else "0"


def build_svg(samples, title):
    width = 960
    height = 540
    margin_left = 90
    margin_right = 30
    margin_top = 60
    margin_bottom = 70
    plot_width = width - margin_left - margin_right
    plot_height = height - margin_top - margin_bottom

    times_s = [item["time_ms"] / 1000.0 for item in samples]
    depths_m = [item["depth_m"] for item in samples]

    max_time = max(times_s) if times_s else 1.0
    max_depth = max(depths_m) if depths_m else 1.0
    if max_time <= 0:
        max_time = 1.0
    if max_depth <= 0:
        max_depth = 1.0

    def x_pos(time_s):
        return margin_left + (time_s / max_time) * plot_width

    def y_pos(depth_m):
        return margin_top + (depth_m / max_depth) * plot_height

    points = " ".join(
        f"{x_pos(time_s):.1f},{y_pos(depth_m):.1f}"
        for time_s, depth_m in zip(times_s, depths_m)
    )

    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
        '<rect width="100%" height="100%" fill="#f8fbfc"/>',
        f'<text x="{width / 2:.1f}" y="32" text-anchor="middle" font-size="22" fill="#12343b">{escape(title)}</text>',
    ]

    for i in range(6):
        tick_time = max_time * i / 5.0
        x = x_pos(tick_time)
        parts.append(
            f'<line x1="{x:.1f}" y1="{margin_top}" x2="{x:.1f}" y2="{margin_top + plot_height}" stroke="#d7e3e7" stroke-width="1"/>'
        )
        parts.append(
            f'<text x="{x:.1f}" y="{height - 24}" text-anchor="middle" font-size="12" fill="#48656c">{format_label(tick_time)}</text>'
        )

    for i in range(6):
        tick_depth = max_depth * i / 5.0
        y = y_pos(tick_depth)
        parts.append(
            f'<line x1="{margin_left}" y1="{y:.1f}" x2="{margin_left + plot_width}" y2="{y:.1f}" stroke="#d7e3e7" stroke-width="1"/>'
        )
        parts.append(
            f'<text x="{margin_left - 14}" y="{y + 4:.1f}" text-anchor="end" font-size="12" fill="#48656c">{format_label(tick_depth)}</text>'
        )

    parts.extend(
        [
            f'<line x1="{margin_left}" y1="{margin_top}" x2="{margin_left}" y2="{margin_top + plot_height}" stroke="#12343b" stroke-width="2"/>',
            f'<line x1="{margin_left}" y1="{margin_top + plot_height}" x2="{margin_left + plot_width}" y2="{margin_top + plot_height}" stroke="#12343b" stroke-width="2"/>',
            f'<text x="{width / 2:.1f}" y="{height - 8}" text-anchor="middle" font-size="14" fill="#12343b">Time (s)</text>',
            f'<text x="24" y="{height / 2:.1f}" text-anchor="middle" font-size="14" fill="#12343b" transform="rotate(-90 24 {height / 2:.1f})">Depth (m)</text>',
        ]
    )

    if points:
        parts.append(
            f'<polyline fill="none" stroke="#0b7285" stroke-width="3" stroke-linejoin="round" stroke-linecap="round" points="{points}"/>'
        )

    parts.append("</svg>")
    return "\n".join(parts)


def main():
    parser = argparse.ArgumentParser(
        description="Plot depth-time data exported from MQTT JSON messages into an SVG."
    )
    parser.add_argument("input", help="Path to a JSON or JSONL file.")
    parser.add_argument(
        "--output",
        help="Output SVG path. Defaults to <input_stem>_depth.svg next to the input file.",
    )
    parser.add_argument(
        "--title",
        default="Depth vs Time",
        help="Chart title written into the SVG.",
    )
    args = parser.parse_args()

    input_path = Path(args.input)
    samples = load_samples(input_path)

    output_path = Path(args.output) if args.output else input_path.with_name(
        f"{input_path.stem}_depth.svg"
    )

    svg = build_svg(samples, args.title)
    output_path.write_text(svg, encoding="utf-8")

    print(f"loaded {len(samples)} samples")
    print(f"saved figure to {output_path}")


if __name__ == "__main__":
    main()
