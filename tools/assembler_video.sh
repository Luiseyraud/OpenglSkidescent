#!/usr/bin/env bash
#------------------------------------------------------------------------------
# assembler_video.sh : produit la vidéo finale video/rendu.mp4 =
#   1. video/compilation.mp4  (capture d'écran OBS : le "make", puis le lancement)
#   2. video/visite.mp4       (produite par : ./skieur --record video/visite.mp4)
#
# À lancer depuis la racine du projet :   bash tools/assembler_video.sh
# Sans video/compilation.mp4, la vidéo finale est la visite seule.
#------------------------------------------------------------------------------
set -e

# Chaque morceau est mis en 1920x1080 à 60 images/s
# (la capture OBS peut avoir une autre taille : elle est réduite et centrée).
NORMALISE="scale=1920:1080:force_original_aspect_ratio=decrease,pad=1920:1080:(ow-iw)/2:(oh-ih)/2,fps=60,setsar=1"
if [ -f video/compilation.mp4 ]; then
	ffmpeg -v error -y -i video/compilation.mp4 -i video/visite.mp4 \
		-filter_complex "[0:v]$NORMALISE[a];[1:v]$NORMALISE[b];[a][b]concat=n=2:v=1:a=0[v]" \
		-map "[v]" -c:v libx264 -pix_fmt yuv420p -crf 18 video/rendu.mp4
	echo "Vidéo finale : video/rendu.mp4 (compilation + visite)"
else
	ffmpeg -v error -y -i video/visite.mp4 -vf "$NORMALISE" -c:v libx264 -pix_fmt yuv420p -crf 18 video/rendu.mp4
	echo "Attention : video/compilation.mp4 absente (capture OBS à faire)."
	echo "Vidéo provisoire : video/rendu.mp4 (visite seule)"
fi
