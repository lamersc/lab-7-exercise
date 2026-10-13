#ifndef ENTITY_MANAGER_H
#define ENTITY_MANAGER_H

#include <cstddef>  // size_t

#include "pack.h"  // Pack from prep

using EntityID = size_t;

template <typename... ComponentTypes>
class EntityManager {};

#endif  // ENTITY_MANAGER_H
