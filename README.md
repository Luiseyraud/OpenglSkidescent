# La descente du skieur

Projet OpenGL (3.3 core) : une animation d'environ une minute, à la tombée de la nuit. Un skieur descend une piste de super-G éclairée par des projecteurs, fait un backflip au ralenti à contre-jour devant le soleil couchant, puis arrive devant les tribunes.

Le terrain, le ciel, le public, les filets, la trace des skis et les effets de neige sont générés par le code. Les autres modèles sont téléchargés (voir les crédits).

## Compiler et lancer

Sous Linux (testé sur Ubuntu 24.04, sous WSL2) :

```bash
sudo apt install build-essential libglfw3-dev libglew-dev libglm-dev ffmpeg
make
./skieur
```

Le programme se lance depuis la racine du projet, car il charge `shaders/`, `models/` et `textures/`.

| Commande | Effet |
|---|---|
| `./skieur` | mode interactif |
| `./skieur --record video/visite.mp4` | rend toute la vidéo en 1920×1080 à 60 images/s, hors temps réel (ffmpeg) |
| `./skieur --frame <t> image.png` | rend l'image de l'instant `t` (en secondes) |

## Commandes du mode interactif

| Touche | Action |
|---|---|
| C | caméra animée / caméra libre |
| Espace | lecture / pause |
| ← / → | reculer / avancer de 2 s |
| Souris, W/A/S/D, Z/X | se déplacer (caméra libre ; Maj : 10 fois plus vite) |
| F1 | fil de fer |
| P | affiche la commande `--frame` qui reproduit la vue |
| Échap | quitter |

## Organisation

- `src/` : le programme.
  - `main.cpp` : les trois modes.
  - `Scene.h` : tous les réglages de la scène.
  - `Timeline.*` : le storyboard. Le skieur, la caméra et le ralenti y sont des fonctions du temps.
  - `World.*` : le chargement et le dessin de la scène.
  - Un module par élément : `Terrain`, `Sky`, `Course`, `Stands`, `Crowd`, `SkiTrail`, `SnowSpray`, `Snowfall`, `Glow`, `PostProcess` (HDR, bloom, tone mapping)…
  - `Mesh`, `Texture2D`, `ShaderProgram` et `Camera` sont les classes du cours. Les changements y sont marqués `AJOUT` ou `CORRECTION`.
- `shaders/` : les shaders GLSL.
- `models/`, `textures/` : les modèles convertis pour le chargeur OBJ du cours.
- `tools/` :
  - `convertir_modeles.sh` reconvertit les modèles à partir des fichiers téléchargés (non fournis) ;
  - `assembler_video.sh` assemble la vidéo finale.

## Crédits

**Code de base :** cours *Learn Modern OpenGL Programming* (Steve Jones, Game Institute / Packt), [dépôt GitHub](https://github.com/PacktPublishing/Learn-Modern-OpenGL-Programming), licence MIT. Bibliothèques [stb_image et stb_image_write](https://github.com/nothings/stb) de Sean Barrett (domaine public).

**Modèles et textures :**

| Élément | Œuvre | Auteur | Licence |
|---|---|---|---|
| Skieur | [Skier](https://poly.pizza/m/6IOZN0Q6lHt) | apelab | CC-BY 3.0 |
| Drapeaux | [Flag 1](https://poly.pizza/m/3l553SpYcc5) (recoloré en rouge) | Chris Daniel | CC-BY 3.0 |
| Chalet | [Snowy North American Chalet Cabin – Low Poly](https://sketchfab.com/3d-models/snowy-north-american-chalet-cabin-low-poly-44ed4093f2904361898b9cbc3dcc1aa9) | ARTEL_3D | CC-BY 4.0 |
| Pylônes d'éclairage | [Stadium Light](https://sketchfab.com/3d-models/stadium-light-425625d154084a29a194dd2b0bb52fd5) | thundermind | CC-BY 4.0 |
| Tribunes | [Seating](https://sketchfab.com/3d-models/seating-9a7040f72e264471a4086a1249822431) | Krish | CC-BY 4.0 |
| Sapins | [Pine Tree with Snow](https://poly.pizza/m/17vQv2X5rh) | Quaternius | CC0 |
| Rochers | [Rock Snow](https://poly.pizza/m/eZRzCg5BcR) | Quaternius | CC0 |
| Neige | [Snow 02](https://polyhaven.com/a/snow_02) | Rob Tuytel (Poly Haven) | CC0 |
| Lampadaires, ampoules | modèles de la démo du cours | *Learn Modern OpenGL Programming* | voir le dépôt du cours |

Les modèles ont été convertis : triangulation, mise à l'échelle, et couleurs des matériaux regroupées dans une texture palette.
