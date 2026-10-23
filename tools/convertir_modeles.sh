#!/usr/bin/env bash
#------------------------------------------------------------------------------
# convertir_modeles.sh : prépare tous les modèles et textures du projet
# à partir des fichiers téléchargés, rangés dans models/sources/.
#
# À lancer depuis la racine du projet :   bash tools/convertir_modeles.sh
#
# Outils nécessaires :
#  - python3 (bibliothèque standard seulement) ;
#  - ffmpeg (réduction et retouche des textures) ;
#  - assimp, pour convertir le FBX et le glTF en OBJ. Sans sudo, on peut
#    l'installer dans un dossier local :
#        mkdir -p ~/outils/assimp && cd ~/outils/assimp
#        apt-get download assimp-utils libassimp5 libdraco8 libminizip1t64 libpugixml1v5
#        for d in *.deb; do dpkg -x "$d" .; done
#    puis lancer ce script avec :
#        ASSIMP_DIR=~/outils/assimp bash tools/convertir_modeles.sh
#------------------------------------------------------------------------------
set -e

SRC=models/sources
TMP=$(mktemp -d)

# Commande assimp (installée dans le système, ou dans ASSIMP_DIR)
if [ -n "$ASSIMP_DIR" ]; then
	assimp() { LD_LIBRARY_PATH="$ASSIMP_DIR/usr/lib/x86_64-linux-gnu" "$ASSIMP_DIR/usr/bin/assimp" "$@"; }
fi

#--- 1. FBX et glTF -> OBJ (-ptv : applique les transformations des nœuds, -tri : triangule)
assimp export "$SRC/PineTree_Snow_4.fbx" "$TMP/sapin.obj" -ptv -tri > /dev/null
assimp export "$SRC/Rock_Snow_2.fbx"     "$TMP/rocher.obj" -ptv -tri > /dev/null
assimp export "$SRC/snowy_north_american_chalet_cabin__low_poly/scene.gltf" "$TMP/chalet.obj" -ptv -tri > /dev/null
assimp export "$SRC/stadium light.fbx"   "$TMP/pylone.obj" -ptv -tri > /dev/null
assimp export "$SRC/Seating.fbx"         "$TMP/tribune.obj" -ptv -tri > /dev/null

#--- 2. Nettoyage pour le chargeur du cours
# Skieur (Poly Pizza) : 18 couleurs de matériaux -> texture palette.
# Le modèle regarde vers -Z : on le tourne de 180° pour qu'il regarde vers +Z (sens de la descente).
# Ses skis montent de 48° vers l'avant (il a été dessiné sur une pente raide) : on le
# tourne de 48° autour de X pour les mettre à plat. Échelle 0.31 : skis de 1,80 m.
python3 tools/convert_model.py "$SRC/skier.obj" skieur --mtl "$SRC/materials.mtl" --scale 0.31 --palette --rotate-y 180 --rotate-x 48

# Sapin et rocher (Quaternius) : couleurs -> palette. La neige des modèles
# (0.40, 0.36, 0.42) est éclaircie pour ressembler à celle du terrain (0.9).
python3 tools/convert_model.py "$TMP/sapin.obj"  sapin  --height 1.0 --palette --color "Snow=0.85,0.85,0.88"
python3 tools/convert_model.py "$TMP/rocher.obj" rocher --height 1.0 --palette --color "Snow.001=0.85,0.85,0.88"

# Drapeau (Poly Pizza) : un piquet blanc et un fanion -> palette. Hauteur 1,80 m.
# Le fanion d'origine est rouge orangé : on lui donne le rouge des filets
# (NET_COLOR = 200, 20, 20 en sRGB, soit 0.577, 0.007, 0.007 en linéaire).
python3 tools/convert_model.py "$SRC/Flag 1 by Chris Daniel - 3l553SpYcc5/model.obj" drapeau --height 1.8 --palette --color "mat14=0.577,0.007,0.007"

# Pylône d'éclairage (Sketchfab), 27 m dans le fichier -> 15 m. On enlève le sol
# fourni avec le modèle (matériau DefaultMaterial), et on le coupe en deux parties
# qui gardent la même origine et la même échelle (--only) :
#  - pylone : le mât (Material, noir -> gris galvanisé) et les boîtiers (Material.001,
#    noir -> gris très foncé), éclairés normalement ;
#  - projecteurs : les vitres (Material.004), dessinées allumées (couleur unie).
python3 tools/convert_model.py "$TMP/pylone.obj" pylone --height 15 --skip DefaultMaterial --palette \
	--only Material --only Material.001 --color "Material=0.30,0.31,0.33" --color "Material.001=0.03,0.03,0.035"
python3 tools/convert_model.py "$TMP/pylone.obj" projecteurs --height 15 --skip DefaultMaterial --only Material.004

# Tribune (Sketchfab, 770 000 triangles). Unité du fichier : environ 0,56 mm (un
# siège fait 888 unités, soit 50 cm). Ses 192 sièges sont identiques : on garde la
# structure (marches, rambardes) et UN siège, plus la position de chaque siège
# (models/sieges.txt). Le programme dessine le même siège à chaque place (instancing).
python3 tools/convert_model.py "$TMP/tribune.obj" tribune --scale 0.00056 --palette \
	--only Poliigon_CeramicPotteryGlazed_10861_2K --only Poliigon_MetalGalvanizedZinc_7184_2K --only Material.003 \
	--color "Poliigon_CeramicPotteryGlazed_10861_2K=0.42,0.42,0.43" --color "Poliigon_MetalGalvanizedZinc_7184_2K=0.55,0.56,0.58"
python3 tools/extraire_sieges.py "$TMP/tribune.obj" siege --scale 0.00056 \
	--seat Material --seat Material.001 --seat Material.002

# Chalet (Sketchfab) : une vraie texture, on garde ses coordonnées UV
python3 tools/convert_model.py "$TMP/chalet.obj" chalet --scale 1.3

#--- 3. Textures
# Chalet : 4096 -> 2048 pixels
ffmpeg -v error -y -i "$SRC/snowy_north_american_chalet_cabin__low_poly/textures/Material.002_baseColor.png" \
	-vf scale=2048:2048 -q:v 2 textures/chalet.jpg

# Neige (Poly Haven snow_02) : 4096 -> 2048, sans couleur. La photo est grise
# (moyenne 170/255) avec des traces sombres qui se voient en se répétant :
# on baisse le contraste (x 0.4) et on éclaircit (moyenne ~240/255).
ffmpeg -v error -y -i "$SRC/snow_02_4k.blend/textures/snow_02_diff_4k.jpg" \
	-vf "scale=2048:2048,hue=s=0,eq=contrast=0.4:brightness=0.37" -q:v 2 textures/neige.jpg

rm -rf "$TMP"
echo "Modèles et textures prêts."
