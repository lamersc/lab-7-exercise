#ifndef REGISTRY_H
#define REGISTRY_H

#include <cassert>
#include <cstddef>
#include <utility>
#include <vector>

template <typename T>
class Registry {
public:
    template <typename U> // type system will try to match with U&& from my understanding
    size_t store(U&& value) {
        if (!free.empty()) {
            size_t id = free.back();
            free.pop_back();
            data[id] = std::forward<U>(value);
            alive[id] = true;
            active++;
            return id;
        }
        data.push_back(std::forward<U>(value));
        alive.push_back(true);
        active++;
        return data.size() - 1;
    }

    void remove(size_t id) {
        assert(valid(id));
        data[id] = T{};
        alive[id] = false;
        free.push_back(id);
        active--;
    }

    T& operator[](size_t id) {
        assert(valid(id));
        return data[id];
    }

    const T& operator[](size_t id) const {
        assert(valid(id));
        return data[id];
    }

    bool valid(size_t id) const {
        return id < alive.size() && alive[id];
    }

    size_t size() const {
        return active;
    }

private:
    std::vector<T> data;
    std::vector<bool> alive;
    std::vector<size_t> free;
    size_t active = 0;
};

#endif  // REGISTRY_H
