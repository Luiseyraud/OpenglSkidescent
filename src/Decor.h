//-----------------------------------------------------------------------------
// Decor.h : placement des sapins et des rochers.
//
// Les positions sont tirées au hasard, puis gardées seulement si l'endroit
// convient (pas sur la piste, pas trop raide, etc.). La graine du hasard est
// fixe : le décor est le même à chaque lancement, donc dans chaque image.
// Chaque position devient une matrice "model", pour l'instancing.
//-----------------------------------------------------------------------------
#ifndef DECOR_H
#define DECOR_H

#include <vector>
#include "glm/glm.hpp"
#include "Terrain.h"

// keepClear : positions à laisser libres (pied des pylônes d'éclairage)
std::vector<glm::mat4> placeTrees(const Terrain& terrain, const std::vector<glm::vec3>& keepClear);
std::vector<glm::mat4> placeRocks(const Terrain& terrain, const std::vector<glm::vec3>& keepClear);

#endif // DECOR_H
