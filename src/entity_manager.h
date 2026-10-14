#ifndef ENTITY_MANAGER_H
#define ENTITY_MANAGER_H

#include <array>
#include <type_traits>
#include <vector>
#include <cassert>
#include <cstddef>
#include <limits>
#include <tuple>

#include "pack.h"
#include "registry.h"

using EntityID = size_t;

template <typename... ComponentTypes>
class EntityManager {
public:
    EntityID createEntity() {
        if (!freeIDs.empty()) {
            EntityID id = freeIDs.back();
            freeIDs.pop_back();
            alive[id] = true;
            for (auto& column : componentIndexes) {
                column[id] = INVALID_INDEX;
            }
            return id;
        }
        EntityID id = alive.size();
        alive.push_back(true);
        for (auto& column : componentIndexes) {
            column.push_back(INVALID_INDEX);
        }
        return id;
    }

    bool validEntity(EntityID id) const {
        return id < alive.size() && alive[id];
    }

    template <typename ComponentType>
    bool hasComponent(EntityID id) const {
        static_assert(Pack<ComponentTypes...>::template contains<ComponentType>,
                      "ComponentType must match one of the component type");
        assert(validEntity(id));
        static constexpr auto COMPONENT_TYPE_INDEX =
            Pack<ComponentTypes...>::template index<ComponentType>;
        return componentIndexes[COMPONENT_TYPE_INDEX][id] != INVALID_INDEX;
    }

    template <typename ComponentType>
    ComponentType& getComponent(EntityID id) {
        static_assert(Pack<ComponentTypes...>::template contains<ComponentType>,
                      "ComponentType must match one of the component type");
        assert(hasComponent<ComponentType>(id));
        static constexpr auto COMPONENT_TYPE_INDEX =
            Pack<ComponentTypes...>::template index<ComponentType>;
        size_t componentIndex = componentIndexes[COMPONENT_TYPE_INDEX][id];
        return std::get<COMPONENT_TYPE_INDEX>(registries)[componentIndex];
    }

    template <typename ComponentType>
    void addComponent(EntityID id, ComponentType&& component) {
        using Type = std::remove_reference_t<ComponentType>;
        static_assert(Pack<ComponentTypes...>::template contains<Type>,
                      "ComponentType must match one of the component type");
        assert(validEntity(id));
        assert(!hasComponent<Type>(id));
        static constexpr auto COMPONENT_TYPE_INDEX =
            Pack<ComponentTypes...>::template index<Type>;
        size_t componentIndex =
            std::get<COMPONENT_TYPE_INDEX>(registries).store(
                std::forward<ComponentType>(component));
        componentIndexes[COMPONENT_TYPE_INDEX][id] = componentIndex;
    }

private:
    static constexpr size_t INVALID_INDEX = std::numeric_limits<size_t>::max();

    std::tuple<Registry<ComponentTypes>...> registries;
    std::array<std::vector<size_t>, sizeof...(ComponentTypes)> componentIndexes;
    std::vector<bool> alive;
    std::vector<size_t> freeIDs;
};

#endif  // ENTITY_MANAGER_H
