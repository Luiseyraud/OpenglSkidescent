#!/usr/bin/env python3
"""
extraire_sieges.py : sépare les sièges d'un modèle de tribune.

Dans le modèle de tribune, chaque siège est un morceau de maillage séparé (il
ne partage aucun sommet avec les autres), et tous les sièges sont identiques.
Ce script :
  1. regroupe les triangles des matériaux de sièges en morceaux connexes
     (deux triangles sont dans le même morceau s'ils partagent un sommet) ;
  2. écrit UN siège dans models/<nom>.obj (origine au pied, au centre) ;
  3. écrit la position (pied, centre) de chaque siège dans models/<nom>s.txt,
     dans le même repère que la tribune convertie par convert_model.py avec la
     même échelle : origine au centre en x et z et en bas en y du modèle ENTIER.
     Chaque ligne : x y z section (section = numéro du matériau, pour la couleur).

Le programme dessine ensuite le même siège à toutes ces positions (instancing) :
c'est beaucoup plus léger que le modèle d'origine (770 000 triangles).

Exemple :
  python3 tools/extraire_sieges.py tribune_source.obj siege --scale 0.00056 \\
      --seat Material --seat Material.001 --seat Material.002
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from convert_model import parse_obj, normalize, cross, sub   # mêmes outils que convert_model.py


def pieces(triangles):
    """Regroupe des triangles en morceaux connexes (union-find sur les sommets)."""
    parent = {}

    def find(a):
        while parent.setdefault(a, a) != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a

    for _, tri in triangles:
        root = find(tri[0][0])
        for corner in tri[1:]:
            other = find(corner[0])
            if other != root:
                parent[other] = root
    groups = {}
    for t in triangles:
        groups.setdefault(find(t[1][0][0]), []).append(t)
    return list(groups.values())


def bounds(positions, tris):
    pts = [positions[c[0]] for _, tri in tris for c in tri]
    lo = [min(p[k] for p in pts) for k in range(3)]
    hi = [max(p[k] for p in pts) for k in range(3)]
    return lo, hi


def main():
    p = argparse.ArgumentParser(description="Sépare les sièges d'une tribune.")
    p.add_argument("input", help="fichier .obj de la tribune entière")
    p.add_argument("name", help="nom de sortie : models/<name>.obj et models/<name>s.txt")
    p.add_argument("--scale", type=float, required=True, help="facteur d'échelle (le même que pour la tribune)")
    p.add_argument("--seat", action="append", required=True, metavar="MAT", help="matériau des sièges (dans l'ordre des sections)")
    p.add_argument("--root", default=".", help="racine du projet")
    args = p.parse_args()

    positions, uvs, normals, triangles, _ = parse_obj(args.input)

    # Origine du modèle entier (comme convert_model.py)
    lo, hi = bounds(positions, triangles)
    origin = ((lo[0] + hi[0]) / 2, lo[1], (lo[2] + hi[2]) / 2)

    seats = []          # (pied du siège dans le repère de la tribune, section)
    reference = None    # triangles du premier siège, et son pied (repère du fichier)
    for section, mat in enumerate(args.seat):
        found = pieces([t for t in triangles if t[0] == mat])
        for piece in found:
            plo, phi = bounds(positions, piece)
            foot = ((plo[0] + phi[0]) / 2, plo[1], (plo[2] + phi[2]) / 2)
            seats.append((tuple((foot[k] - origin[k]) * args.scale for k in range(3)), section))
            if reference is None:
                reference = (piece, foot, [phi[k] - plo[k] for k in range(3)])
            else:
                size = [phi[k] - plo[k] for k in range(3)]
                if max(abs(size[k] - reference[2][k]) for k in range(3)) > 1e-3 * max(reference[2]):
                    print(f"  attention : un siège de {mat} n'a pas la même taille que les autres")
        print(f"  {mat} : {len(found)} sièges")

    # Un siège, origine à son pied
    piece, foot, size = reference
    obj_path = os.path.join(args.root, "models", args.name + ".obj")
    with open(obj_path, "w") as f:
        f.write(f"# Un siège extrait de {os.path.basename(args.input)} par tools/extraire_sieges.py\n")
        out_v, out_vn, faces = {}, {}, []

        def key(table, value):
            value = tuple(round(x, 6) for x in value)
            if value not in table:
                table[value] = len(table) + 1
            return table[value]

        for _, tri in piece:
            pts = [tuple((positions[c[0]][k] - foot[k]) * args.scale for k in range(3)) for c in tri]
            face_normal = normalize(cross(sub(pts[1], pts[0]), sub(pts[2], pts[0])))
            corners = []
            for c, pos in zip(tri, pts):
                n = normals[c[2]] if c[2] is not None else face_normal
                corners.append(f"{key(out_v, pos)}/1/{key(out_vn, normalize(n))}")
            faces.append("f " + " ".join(corners))
        f.writelines(f"v {x:.6f} {y:.6f} {z:.6f}\n" for x, y, z in out_v)
        f.write("vt 0.5 0.5\n")     # couleur unie : une seule coordonnée de texture
        f.writelines(f"vn {x:.6f} {y:.6f} {z:.6f}\n" for x, y, z in out_vn)
        f.write("\n".join(faces) + "\n")
    print(f"{obj_path} : {len(piece)} triangles, taille "
          f"{size[0] * args.scale:.2f} x {size[1] * args.scale:.2f} x {size[2] * args.scale:.2f} m")

    txt_path = os.path.join(args.root, "models", args.name + "s.txt")
    with open(txt_path, "w") as f:
        f.write("# Pied de chaque siège dans le repère de la tribune (m) : x y z section\n")
        for (x, y, z), section in seats:
            f.write(f"{x:.4f} {y:.4f} {z:.4f} {section}\n")
    print(f"{txt_path} : {len(seats)} sièges")


if __name__ == "__main__":
    main()
