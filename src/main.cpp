#include <cassert>
#include <iostream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "entity_manager.h"
#include "registry.h"

int main() {
    // Types used only by these tests. Defined locally so that every
    // assert below lives directly in main, as required.
    struct Widget {
        int value = 0;
        std::string name;
        bool operator==(const Widget& other) const {
            return value == other.value && name == other.name;
        }
    };

    struct Counted {
        int value = 0;
        Counted() = default;
        explicit Counted(int v) : value(v) {}
        Counted(const Counted& other) : value(other.value) {}
        Counted(Counted&& other) noexcept : value(other.value) {
            other.value = -1;
        }
        Counted& operator=(const Counted& other) {
            value = other.value;
            return *this;
        }
        Counted& operator=(Counted&& other) noexcept {
            value = other.value;
            other.value = -1;
            return *this;
        }
    };

    struct Position {
        float x = 0.0f;
        float y = 0.0f;
        bool operator==(const Position& other) const {
            return x == other.x && y == other.y;
        }
    };

    struct Velocity {
        float dx = 0.0f;
        float dy = 0.0f;
    };

    // ================================================================
    // Registry: empty state and validity of arbitrary tickets
    // ================================================================
    {
        Registry<int> reg;
        assert(reg.size() == 0);
        assert(!reg.valid(0));
        assert(!reg.valid(1));
        assert(!reg.valid(999));
        assert(!reg.valid(std::numeric_limits<size_t>::max()));
    }

    // ================================================================
    // Registry: sequential tickets, copy/move store, operator[]
    // ================================================================
    {
        Registry<int> reg;

        int source = 42;
        size_t id0 = reg.store(source);  // store(const T&): copies
        assert(id0 == 0);
        assert(reg.size() == 1);
        assert(reg.valid(id0));
        assert(reg[id0] == 42);
        // Copy semantics: mutating the source must not affect the copy.
        source = 99;
        assert(reg[id0] == 42);

        size_t id1 = reg.store(7);  // store(T&&)
        assert(id1 == 1);
        assert(reg[id1] == 7);

        int third = 30;
        size_t id2 = reg.store(std::move(third));
        assert(id2 == 2);
        assert(reg[id2] == 30);

        assert(reg.size() == 3);
        assert(reg.valid(0) && reg.valid(1) && reg.valid(2));
        assert(!reg.valid(3));

        // Mutable operator[] returns a true reference.
        reg[id0] = 100;
        assert(reg[id0] == 100);
        reg[id1] += 1;
        assert(reg[id1] == 8);

        // Const operator[].
        const Registry<int>& constReg = reg;
        assert(constReg[id0] == 100);
        assert(constReg[id2] == 30);
        assert(constReg.size() == 3);
        assert(constReg.valid(id1));
    }

    // ================================================================
    // Registry: remove, validity, size, and ticket recycling
    // (recycled tickets are preferred over expanding storage)
    // ================================================================
    {
        Registry<int> reg;
        size_t a = reg.store(10);
        size_t b = reg.store(20);
        size_t c = reg.store(30);
        assert(a == 0 && b == 1 && c == 2);
        assert(reg.size() == 3);

        reg.remove(b);
        assert(reg.size() == 2);
        assert(!reg.valid(b));
        assert(reg.valid(a) && reg.valid(c));
        assert(reg[a] == 10 && reg[c] == 30);

        // The recycled ticket is handed out before any fresh ticket.
        size_t reused = reg.store(200);
        assert(reused == b);
        assert(reg.valid(reused));
        assert(reg[reused] == 200);
        assert(reg.size() == 3);

        // With no recycled tickets left, storage expands sequentially.
        size_t fresh = reg.store(40);
        assert(fresh == 3);
        assert(reg[fresh] == 40);
        assert(reg.size() == 4);

        // Remove two tickets; both must be recycled (in some order)
        // before a brand-new ticket is issued.
        reg.remove(a);
        reg.remove(c);
        assert(reg.size() == 2);
        assert(!reg.valid(a) && !reg.valid(c));

        size_t r1 = reg.store(1000);
        size_t r2 = reg.store(2000);
        assert((r1 == a || r1 == c));
        assert((r2 == a || r2 == c));
        assert(r1 != r2);
        assert(reg[r1] == 1000 || reg[r1] == 2000);
        assert(reg[r2] == 1000 || reg[r2] == 2000);
        assert(reg.size() == 4);

        size_t after = reg.store(50);
        assert(after == 4);
        assert(reg.size() == 5);
    }

    // ================================================================
    // Registry: interleaved store/remove keeps other entries intact
    // ================================================================
    {
        Registry<std::string> reg;
        size_t a = reg.store(std::string("alpha"));
        size_t b = reg.store(std::string("bravo"));
        size_t c = reg.store(std::string("charlie"));
        assert(reg.size() == 3);

        reg.remove(a);
        assert(reg[b] == "bravo");
        assert(reg[c] == "charlie");

        size_t d = reg.store(std::string("delta"));
        assert(d == a);
        assert(reg[d] == "delta");
        assert(reg[b] == "bravo");
        assert(reg[c] == "charlie");

        // Copy store of an lvalue string leaves the source intact.
        std::string word = "echo";
        size_t e = reg.store(word);
        assert(word == "echo");
        assert(reg[e] == "echo");

        // Move store of a string stores the correct value.
        std::string movable = "foxtrot";
        size_t f = reg.store(std::move(movable));
        assert(reg[f] == "foxtrot");
        assert(reg.size() == 5);
    }

    // ================================================================
    // Registry: copy vs move is observable via Counted
    // ================================================================
    {
        Registry<Counted> reg;

        Counted original{5};
        size_t id0 = reg.store(original);  // lvalue -> copy
        assert(original.value == 5);  // copy leaves the source untouched
        assert(reg[id0].value == 5);

        Counted movable{6};
        size_t id1 = reg.store(std::move(movable));  // rvalue -> move
        assert(movable.value == -1);  // moved-from sentinel
        assert(reg[id1].value == 6);

        // Recycling also supports move assignment into the freed slot.
        reg.remove(id0);
        Counted replacement{7};
        size_t id2 = reg.store(std::move(replacement));
        assert(id2 == id0);
        assert(replacement.value == -1);
        assert(reg[id2].value == 7);
    }

    // ================================================================
    // Registry: custom value type and mutation through operator[]
    // ================================================================
    {
        Registry<Widget> reg;
        Widget w{1, "one"};
        size_t a = reg.store(w);
        size_t b = reg.store(Widget{2, "two"});
        assert(reg[a] == (Widget{1, "one"}));
        assert(reg[b] == (Widget{2, "two"}));
        reg[a].value = 10;
        reg[a].name = "ten";
        assert(reg[a] == (Widget{10, "ten"}));
        assert(reg[b] == (Widget{2, "two"}));
    }

    // ================================================================
    // Registry: heap-owning element type, removed slot is reused
    // with the new value (not the stale one)
    // ================================================================
    {
        Registry<std::vector<int>> reg;
        size_t a = reg.store(std::vector<int>{1, 2, 3});
        size_t b = reg.store(std::vector<int>{4, 5});
        assert(reg[a] == (std::vector<int>{1, 2, 3}));
        assert(reg[b] == (std::vector<int>{4, 5}));
        reg.remove(a);
        assert(!reg.valid(a));
        size_t c = reg.store(std::vector<int>{9});
        assert(c == a);
        assert(reg[c] == (std::vector<int>{9}));
        assert(reg[b] == (std::vector<int>{4, 5}));
    }

    // ================================================================
    // Registry: bulk store/remove/recycle stress
    // ================================================================
    {
        Registry<int> reg;
        constexpr size_t n = 100;
        for (size_t i = 0; i < n; ++i) {
            size_t id = reg.store(static_cast<int>(i * 3));
            assert(id == i);
        }
        assert(reg.size() == n);
        for (size_t i = 0; i < n; ++i) {
            assert(reg.valid(i));
            assert(reg[i] == static_cast<int>(i * 3));
        }
        assert(!reg.valid(n));

        // Remove every even ticket.
        for (size_t i = 0; i < n; i += 2) {
            reg.remove(i);
        }
        assert(reg.size() == n / 2);
        for (size_t i = 0; i < n; ++i) {
            assert(reg.valid(i) == (i % 2 != 0));
        }

        // Refilling must recycle exactly the freed (even) tickets:
        // no ticket at or beyond n may be issued yet.
        for (size_t k = 0; k < n / 2; ++k) {
            size_t id = reg.store(-1);
            assert(id < n);
            assert(id % 2 == 0);
            assert(reg[id] == -1);
        }
        assert(reg.size() == n);

        // Odd entries survived the whole cycle untouched.
        for (size_t i = 1; i < n; i += 2) {
            assert(reg[i] == static_cast<int>(i * 3));
        }

        // Recycle pool is drained, so the next ticket expands storage.
        assert(reg.store(12345) == n);
        assert(reg.size() == n + 1);
    }

    // ================================================================
    // Registry: independent instances do not share state
    // ================================================================
    {
        Registry<int> first;
        Registry<int> second;
        size_t a = first.store(1);
        assert(first.size() == 1);
        assert(second.size() == 0);
        assert(!second.valid(a));
        size_t b = second.store(2);
        assert(a == 0 && b == 0);
        assert(first[a] == 1 && second[b] == 2);
        first.remove(a);
        assert(!first.valid(a));
        assert(second.valid(b));
    }

    // ================================================================
    // EntityManager: the worked example from the specification
    // ================================================================
    {
        EntityManager<int, float, bool> entityManager;
        EntityID e = entityManager.createEntity();
        entityManager.addComponent(e, 5);  // deduced as addComponent<int>
        assert(entityManager.hasComponent<int>(e));
        assert(!entityManager.hasComponent<float>(e));
        assert(!entityManager.hasComponent<bool>(e));
        assert(entityManager.getComponent<int>(e) == 5);
    }

    // ================================================================
    // EntityManager: entity IDs are sequential, validEntity has
    // no preconditions (any ID may be queried)
    // ================================================================
    {
        EntityManager<int, float> em;
        assert(!em.validEntity(0));
        assert(!em.validEntity(1));
        assert(!em.validEntity(999));
        assert(!em.validEntity(std::numeric_limits<size_t>::max()));

        EntityID e0 = em.createEntity();
        EntityID e1 = em.createEntity();
        EntityID e2 = em.createEntity();
        assert(e0 == 0 && e1 == 1 && e2 == 2);
        assert(em.validEntity(e0));
        assert(em.validEntity(e1));
        assert(em.validEntity(e2));
        assert(!em.validEntity(3));
        assert(!em.validEntity(1000));
    }

    // ================================================================
    // EntityManager: a fresh entity has no components, adding one
    // component does not add the others, getComponent mutates
    // ================================================================
    {
        EntityManager<int, float, bool> em;
        EntityID e = em.createEntity();
        assert(!em.hasComponent<int>(e));
        assert(!em.hasComponent<float>(e));
        assert(!em.hasComponent<bool>(e));

        em.addComponent(e, 2.5f);
        assert(!em.hasComponent<int>(e));
        assert(em.hasComponent<float>(e));
        assert(!em.hasComponent<bool>(e));
        assert(em.getComponent<float>(e) == 2.5f);

        // getComponent returns a mutable reference to stored data.
        em.getComponent<float>(e) = 3.5f;
        assert(em.getComponent<float>(e) == 3.5f);
        float& ref = em.getComponent<float>(e);
        ref += 1.0f;
        assert(em.getComponent<float>(e) == 4.5f);

        em.addComponent(e, true);
        assert(em.hasComponent<bool>(e));
        assert(em.hasComponent<float>(e));

        // Const view: validEntity / hasComponent are callable.
        const EntityManager<int, float, bool>& constEm = em;
        assert(constEm.validEntity(e));
        assert(constEm.hasComponent<float>(e));
        assert(!constEm.hasComponent<int>(e));
    }

    // ================================================================
    // EntityManager: addComponent copies lvalues, moves rvalues
    // ================================================================
    {
        EntityManager<int, std::string> em;
        EntityID e = em.createEntity();

        int number = 11;
        em.addComponent(e, number);  // lvalue -> copy
        assert(number == 11);
        number = 22;
        assert(em.getComponent<int>(e) == 11);

        std::string text = "hello";
        em.addComponent(e, text);  // lvalue string -> copy
        assert(text == "hello");
        assert(em.getComponent<std::string>(e) == "hello");

        EntityID e2 = em.createEntity();
        std::string movable = "world";
        em.addComponent(e2, std::move(movable));  // rvalue -> move
        assert(em.getComponent<std::string>(e2) == "world");
        em.addComponent(e2, 77);
        assert(em.getComponent<int>(e2) == 77);
    }

    // ================================================================
    // EntityManager: entities are independent of each other
    // ================================================================
    {
        EntityManager<int, float> em;
        EntityID a = em.createEntity();
        EntityID b = em.createEntity();
        EntityID c = em.createEntity();

        em.addComponent(a, 1);
        em.addComponent(b, 2);
        em.addComponent(b, 20.0f);

        assert(em.getComponent<int>(a) == 1);
        assert(em.getComponent<int>(b) == 2);
        assert(em.hasComponent<float>(b));
        assert(!em.hasComponent<float>(a));
        assert(!em.hasComponent<int>(c));
        assert(!em.hasComponent<float>(c));

        // Mutating one entity's component never touches another's.
        em.getComponent<int>(a) = 100;
        assert(em.getComponent<int>(a) == 100);
        assert(em.getComponent<int>(b) == 2);
        assert(em.getComponent<float>(b) == 20.0f);
    }

    // ================================================================
    // EntityManager: user-defined component types
    // ================================================================
    {
        EntityManager<Position, Velocity> em;
        EntityID e = em.createEntity();
        assert(!em.hasComponent<Position>(e));
        assert(!em.hasComponent<Velocity>(e));

        em.addComponent(e, Position{1.0f, 2.0f});
        assert(em.hasComponent<Position>(e));
        assert(!em.hasComponent<Velocity>(e));
        assert(em.getComponent<Position>(e) == (Position{1.0f, 2.0f}));

        em.addComponent(e, Velocity{0.5f, -0.5f});
        assert(em.getComponent<Velocity>(e).dx == 0.5f);
        assert(em.getComponent<Velocity>(e).dy == -0.5f);

        em.getComponent<Position>(e).x += 10.0f;
        assert(em.getComponent<Position>(e).x == 11.0f);
        assert(em.getComponent<Position>(e).y == 2.0f);
    }

    // ================================================================
    // EntityManager: many entities with selective components
    // ================================================================
    {
        EntityManager<int, float> em;
        constexpr size_t n = 20;
        std::vector<EntityID> entities;
        for (size_t i = 0; i < n; ++i) {
            entities.push_back(em.createEntity());
            assert(entities.back() == i);
            assert(em.validEntity(entities.back()));
        }
        assert(!em.validEntity(n));

        for (size_t i = 0; i < n; ++i) {
            if (i % 2 == 0) {
                em.addComponent(entities[i], static_cast<int>(i * 10));
            }
            if (i % 3 == 0) {
                em.addComponent(entities[i], static_cast<float>(i) + 0.25f);
            }
        }
        for (size_t i = 0; i < n; ++i) {
            assert(em.hasComponent<int>(entities[i]) == (i % 2 == 0));
            assert(em.hasComponent<float>(entities[i]) == (i % 3 == 0));
            if (i % 2 == 0) {
                assert(em.getComponent<int>(entities[i]) ==
                       static_cast<int>(i * 10));
            }
            if (i % 3 == 0) {
                assert(em.getComponent<float>(entities[i]) ==
                       static_cast<float>(i) + 0.25f);
            }
        }
    }

    // ================================================================
    // EntityManager: same component type, many distinct values;
    // a single-component manager; independent manager instances
    // ================================================================
    {
        EntityManager<int> em;
        EntityID first = em.createEntity();
        EntityID second = em.createEntity();
        em.addComponent(first, -5);
        em.addComponent(second, 0);
        assert(em.getComponent<int>(first) == -5);
        assert(em.getComponent<int>(second) == 0);

        EntityManager<int> other;
        assert(!other.validEntity(first));
        EntityID otherFirst = other.createEntity();
        assert(otherFirst == 0);
        assert(!other.hasComponent<int>(otherFirst));
        assert(em.getComponent<int>(first) == -5);
    }

    std::cout << "All Registry and EntityManager tests passed.\n";
    return 0;
}
