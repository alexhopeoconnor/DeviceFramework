#!/usr/bin/env bash
set -euo pipefail

artifact_dir="${ARTIFACT_DIR:?ARTIFACT_DIR is required}"
portal_video="$artifact_dir/portal/readme-media/raw/portal-tour.webm"
web_video="$artifact_dir/web/readme-media/raw/web-ui-tour.webm"
status_image="$artifact_dir/web/readme-media/device-status.png"
media_dir="$artifact_dir/readme-media"
target_gif="$media_dir/device-lifecycle-tour.gif"

for required in "$portal_video" "$web_video" "$status_image"; do
    [[ -s "$required" ]] || {
        echo "README media output is incomplete: $required" >&2
        exit 1
    }
done

install -d -m 700 "$media_dir"
cp -- "$status_image" "$media_dir/device-status.png"

ffmpeg -hide_banner -loglevel error -y \
    -i "$portal_video" -i "$web_video" \
    -filter_complex '[0:v]setpts=1.25*PTS,fps=5,scale=600:750:force_original_aspect_ratio=decrease,pad=600:750:(ow-iw)/2:(oh-ih)/2,setsar=1[portal];[1:v]setpts=1.25*PTS,fps=5,scale=600:750:force_original_aspect_ratio=decrease,pad=600:750:(ow-iw)/2:(oh-ih)/2,setsar=1[web];[portal][web]concat=n=2:v=1:a=0,split[a][b];[a]palettegen=max_colors=96[p];[b][p]paletteuse' \
    -loop 0 "$target_gif"

[[ -s "$target_gif" ]] || {
    echo "README media GIF was not rendered: $target_gif" >&2
    exit 1
}

duration="$(ffprobe -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 "$target_gif")"
awk -v duration="$duration" 'BEGIN { exit !(duration >= 4 && duration <= 30) }' || {
    echo "README media GIF duration is outside the 4–30 second review range: $duration" >&2
    exit 1
}

max_bytes=$((2 * 1024 * 1024))
(( $(wc -c < "$target_gif") <= max_bytes )) || {
    echo "README media GIF exceeds its 2 MiB documentation budget: $target_gif" >&2
    exit 1
}
