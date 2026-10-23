#!/usr/bin/env python3
"""
convert_model.py : prépare un modèle OBJ pour le chargeur du cours (classe Mesh).

Le chargeur du cours a des limites : que des triangles, une seule texture par
modèle, des normales obligatoires, et les fichiers .mtl sont ignorés. Ce script :
  1. triangule les faces (en éventail : un polygone a,b,c,d donne a,b,c et a,c,d) ;
  2. calcule une normale par face quand le fichier n'en a pas ;
  3. pose l'origine au pied du modèle, centrée, et le met à la bonne taille (en mètres) ;
  4. avec --palette : remplace les couleurs des matériaux (Kd du .mtl) par une
     petite "texture palette" (un carré de couleur par matériau). Chaque sommet
     reçoit les coordonnées UV du carré de son matériau : on obtient UN seul OBJ
     et UNE seule texture, sans rien changer au chargeur ni au shader.

Bibliothèque standard uniquement (pas de numpy ni de Pillow).

Exemple :
  python3 tools/convert_model.py models/sources/skier.obj skieur --height 1.3 --palette
"""
import argparse
import math
import os
import struct
import zlib

BLOCK = 4   # taille en pixels d'un carré de couleur dans la palette


def parse_mtl(path):
    """Lit un .mtl : pour chaque matériau, sa couleur Kd et son éventuelle texture."""
    materials, current = {}, None
    if not path or not os.path.exists(path):
        return materials
    for line in open(path, encoding="utf-8", errors="replace"):
        parts = line.split()
        if not parts:
            continue
        if parts[0] == "newmtl":
            current = " ".join(parts[1:])
            materials[current] = {"Kd": (0.8, 0.8, 0.8), "map_Kd": None}
        elif current and parts[0] == "Kd":
            materials[current]["Kd"] = tuple(float(x) for x in parts[1:4])
        elif current and parts[0] == "map_Kd":
            materials[current]["map_Kd"] = parts[-1]
    return materials


def parse_obj(path):
    """Lit un .obj. Renvoie les positions, UV, normales et la liste des triangles.
    Chaque triangle = (matériau, [(iv, ivt, ivn) x 3]) avec des indices à partir de 0
    (ivt ou ivn valent None s'ils sont absents)."""
    positions, uvs, normals, triangles = [], [], [], []
    material, mtllib = None, None

    def index(token, count):
        # Les indices OBJ commencent à 1 ; un indice négatif compte depuis la fin
        if token == "":
            return None
        i = int(token)
        return i - 1 if i > 0 else count + i

    for line in open(path, encoding="utf-8", errors="replace"):
        parts = line.split()
        if not parts:
            continue
        if parts[0] == "v":
            positions.append(tuple(float(x) for x in parts[1:4]))
        elif parts[0] == "vt":
            uvs.append(tuple(float(x) for x in parts[1:3]))
        elif parts[0] == "vn":
            normals.append(tuple(float(x) for x in parts[1:4]))
        elif parts[0] == "usemtl":
            material = " ".join(parts[1:])
        elif parts[0] == "mtllib":
            mtllib = " ".join(parts[1:])
        elif parts[0] == "f":
            corners = []
            for token in parts[1:]:
                fields = (token.split("/") + ["", ""])[:3]
                corners.append((index(fields[0], len(positions)),
                                index(fields[1], len(uvs)),
                                index(fields[2], len(normals))))
            # Triangulation en éventail
            for k in range(1, len(corners) - 1):
                triangles.append((material, [corners[0], corners[k], corners[k + 1]]))
    return positions, uvs, normals, triangles, mtllib


def linear_to_srgb(c):
    """Couleur linéaire -> sRGB (ce que stocke une image)."""
    c = min(max(c, 0.0), 1.0)
    return 12.92 * c if c <= 0.0031308 else 1.055 * c ** (1 / 2.4) - 0.055


def write_png(path, width, height, pixel):
    """Écrit une image PNG RGB 8 bits. pixel(x, y) renvoie (r, g, b) entre 0 et 255."""
    raw = b"".join(b"\x00" + bytes(v for x in range(width) for v in pixel(x, y)) for y in range(height))

    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)))
        f.write(chunk(b"IDAT", zlib.compress(raw, 9)))
        f.write(chunk(b"IEND", b""))


def sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def normalize(a):
    n = math.sqrt(a[0] ** 2 + a[1] ** 2 + a[2] ** 2) or 1.0
    return (a[0] / n, a[1] / n, a[2] / n)


def main():
    p = argparse.ArgumentParser(description="Prépare un OBJ pour le chargeur du cours.")
    p.add_argument("input", help="fichier .obj d'entrée")
    p.add_argument("name", help="nom de sortie : models/<name>.obj (+ textures/<name>_palette.png)")
    size = p.add_mutually_exclusive_group(required=True)
    size.add_argument("--height", type=float, help="hauteur voulue en mètres")
    size.add_argument("--scale", type=float, help="facteur d'échelle direct")
    p.add_argument("--mtl", help="fichier .mtl (par défaut : celui indiqué par mtllib)")
    p.add_argument("--palette", action="store_true", help="couleurs des matériaux -> texture palette")
    p.add_argument("--kd-srgb", action="store_true", help="les Kd du .mtl sont déjà en sRGB")
    p.add_argument("--color", action="append", default=[], metavar="MAT=R,G,B",
                   help="remplace la couleur (linéaire) d'un matériau")
    p.add_argument("--rotate-y", type=float, default=0.0, help="rotation autour de Y, en degrés (faite en premier)")
    p.add_argument("--rotate-x", type=float, default=0.0, help="rotation autour de X, en degrés (faite ensuite)")
    p.add_argument("--flip-v", action="store_true", help="retourne la coordonnée de texture v")
    p.add_argument("--skip", action="append", default=[], metavar="MAT",
                   help="supprime les triangles de ce matériau (ex. un sol fourni avec le modèle)")
    p.add_argument("--only", action="append", default=[], metavar="MAT",
                   help="n'écrit que les triangles de ce matériau. L'origine et l'échelle restent "
                        "celles du modèle entier : plusieurs parties converties ainsi se superposent")
    p.add_argument("--root", default=".", help="racine du projet (models/ et textures/)")
    args = p.parse_args()

    positions, uvs, normals, triangles, mtllib = parse_obj(args.input)
    triangles = [t for t in triangles if t[0] not in args.skip]
    mtl_path = args.mtl or (os.path.join(os.path.dirname(args.input), mtllib) if mtllib else None)
    materials = parse_mtl(mtl_path)
    for spec in args.color:
        mat, rgb = spec.split("=")
        materials.setdefault(mat, {"map_Kd": None})["Kd"] = tuple(float(x) for x in rgb.split(","))

    # 1. Rotations autour de Y puis de X (positions et normales)
    a = math.radians(args.rotate_y)
    ca, sa = math.cos(a), math.sin(a)
    rot_y = lambda v: (ca * v[0] + sa * v[2], v[1], -sa * v[0] + ca * v[2])
    b = math.radians(args.rotate_x)
    cb, sb = math.cos(b), math.sin(b)
    rot_x = lambda v: (v[0], cb * v[1] - sb * v[2], sb * v[1] + cb * v[2])
    positions = [rot_x(rot_y(v)) for v in positions]
    normals = [rot_x(rot_y(n)) for n in normals]

    # 2. Origine au pied du modèle (centre en x et z, bas en y), puis mise à l'échelle
    used = sorted({c[0] for _, tri in triangles for c in tri})
    lo = [min(positions[i][k] for i in used) for k in range(3)]
    hi = [max(positions[i][k] for i in used) for k in range(3)]
    scale = args.scale if args.scale else args.height / (hi[1] - lo[1])
    origin = ((lo[0] + hi[0]) / 2, lo[1], (lo[2] + hi[2]) / 2)
    positions = [tuple((v[k] - origin[k]) * scale for k in range(3)) for v in positions]

    # --only : on ne garde qu'une partie, APRÈS avoir calculé l'origine et l'échelle
    if args.only:
        triangles = [t for t in triangles if t[0] in args.only]
        kept = sorted({c[0] for _, tri in triangles for c in tri})
        part_lo = [min(positions[i][k] for i in kept) for k in range(3)]
        part_hi = [max(positions[i][k] for i in kept) for k in range(3)]
        print("  partie gardée : de (%.2f, %.2f, %.2f) à (%.2f, %.2f, %.2f) m" % (*part_lo, *part_hi))

    # 3. Palette : un carré de BLOCK x BLOCK pixels par matériau, sur une ligne
    palette = []
    for mat, _ in triangles:
        if mat not in palette:
            palette.append(mat)
    width = BLOCK * len(palette)

    def palette_uv(mat):
        i = palette.index(mat)
        return ((i * BLOCK + BLOCK / 2) / width, 0.5)   # centre du carré

    # 4. Écriture de l'OBJ (sommets, UV et normales dédoublonnés)
    out_v, out_vt, out_vn, out_f = {}, {}, {}, []

    def key(table, value):
        value = tuple(round(x, 6) for x in value)
        if value not in table:
            table[value] = len(table) + 1
        return table[value]

    missing_uv = 0
    for mat, tri in triangles:
        pts = [positions[c[0]] for c in tri]
        face_normal = normalize(cross(sub(pts[1], pts[0]), sub(pts[2], pts[0])))
        corners = []
        for (iv, ivt, ivn), pos in zip(tri, pts):
            n = normals[ivn] if ivn is not None else face_normal
            if args.palette:
                uv = palette_uv(mat)
            elif ivt is not None:
                uv = uvs[ivt]
                if args.flip_v:
                    uv = (uv[0], 1.0 - uv[1])
            else:
                uv = (0.0, 0.0)
                missing_uv += 1
            corners.append(f"{key(out_v, pos)}/{key(out_vt, uv)}/{key(out_vn, normalize(n))}")
        out_f.append("f " + " ".join(corners))

    obj_path = os.path.join(args.root, "models", args.name + ".obj")
    with open(obj_path, "w") as f:
        f.write(f"# Converti par tools/convert_model.py depuis {os.path.basename(args.input)}\n")
        f.write(f"# Triangles uniquement, origine au pied, 1 unité = 1 mètre\n")
        f.writelines(f"v {x:.6f} {y:.6f} {z:.6f}\n" for x, y, z in out_v)
        f.writelines(f"vt {u:.6f} {v:.6f}\n" for u, v in out_vt)
        f.writelines(f"vn {x:.6f} {y:.6f} {z:.6f}\n" for x, y, z in out_vn)
        f.write("\n".join(out_f) + "\n")

    dims = [(hi[k] - lo[k]) * scale for k in range(3)]
    print(f"{obj_path} : {len(out_f)} triangles, {len(palette)} matériau(x), "
          f"taille {dims[0]:.2f} x {dims[1]:.2f} x {dims[2]:.2f} m")
    if missing_uv:
        print(f"  attention : {missing_uv} sommets sans coordonnées de texture")

    if args.palette:
        colors = []
        for mat in palette:
            kd = materials.get(mat, {}).get("Kd", (0.8, 0.8, 0.8))
            srgb = kd if args.kd_srgb else tuple(linear_to_srgb(c) for c in kd)
            colors.append(tuple(int(round(c * 255)) for c in srgb))
            print(f"  {mat:16s} -> {colors[-1]}")
        png_path = os.path.join(args.root, "textures", args.name + "_palette.png")
        write_png(png_path, width, BLOCK, lambda x, y: colors[x // BLOCK])
        print(f"{png_path} : {len(palette)} couleurs")


if __name__ == "__main__":
    main()
