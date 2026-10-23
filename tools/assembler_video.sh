#!/usr/bin/env bash
#------------------------------------------------------------------------------
# assembler_video.sh : produit la vidéo finale video/rendu.mp4 =
#   1. video/compilation.mp4  (capture d'écran OBS : le "make", puis le lancement)
#   2. video/visite.mp4       (produite par : ./skieur --record video/visite.mp4)
#   3. le générique de fin    (texte : tools/generique.txt)
#
# À lancer depuis la racine du projet :   bash tools/assembler_video.sh
# Sans video/compilation.mp4, la vidéo est assemblée sans la partie compilation.
#------------------------------------------------------------------------------
set -e

FONT=/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf

#--- 1. Générique : 6 s de texte blanc sur fond sombre, avec fondu
ffmpeg -v error -y -f lavfi -i "color=c=0x14122a:s=1920x1080:r=60:d=6" \
	-vf "drawtext=fontfile=$FONT:textfile=tools/generique.txt:fontsize=32:fontcolor=white:line_spacing=20:x=(w-text_w)/2:y=(h-text_h)/2,fade=t=in:d=1,fade=t=out:st=5:d=1" \
	-c:v libx264 -pix_fmt yuv420p -crf 18 video/generique.mp4

#--- 2. Assemblage. Chaque morceau est mis en 1920x1080 à 60 images/s
# (la capture OBS peut avoir une autre taille : elle est réduite et centrée).
NORMALISE="scale=1920:1080:force_original_aspect_ratio=decrease,pad=1920:1080:(ow-iw)/2:(oh-ih)/2,fps=60,setsar=1"
if [ -f video/compilation.mp4 ]; then
	ffmpeg -v error -y -i video/compilation.mp4 -i video/visite.mp4 -i video/generique.mp4 \
		-filter_complex "[0:v]$NORMALISE[a];[1:v]$NORMALISE[b];[2:v]$NORMALISE[c];[a][b][c]concat=n=3:v=1:a=0[v]" \
		-map "[v]" -c:v libx264 -pix_fmt yuv420p -crf 18 video/rendu.mp4
	echo "Vidéo finale : video/rendu.mp4 (compilation + visite + générique)"
else
	ffmpeg -v error -y -i video/visite.mp4 -i video/generique.mp4 \
		-filter_complex "[0:v]$NORMALISE[b];[1:v]$NORMALISE[c];[b][c]concat=n=2:v=1:a=0[v]" \
		-map "[v]" -c:v libx264 -pix_fmt yuv420p -crf 18 video/rendu.mp4
	echo "Attention : video/compilation.mp4 absente (capture OBS à faire)."
	echo "Vidéo provisoire : video/rendu.mp4 (visite + générique)"
fi
