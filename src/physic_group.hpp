#pragma once

#include "types.hpp"
#include <SFML/System/Vector2.hpp>
#include <vector>
#include <map>
#include "physic_point.hpp"

namespace dfdh {

class physic_group : public physic_point, public std::enable_shared_from_this<physic_group> {
public:
    friend class group_tree_iterator;

    physic_group(const sf::Vector2f& iposition        = {0.f, 0.f},
                 const sf::Vector2f& idir             = {1.f, 0.f},
                 float               iscalar_velocity = 0.f,
                 float               imass            = 1.f,
                 float               ielasticity      = 0.5f):
        physic_point(iposition, idir, iscalar_velocity, imass, ielasticity) {}

    static std::shared_ptr<physic_group> create(const sf::Vector2f& iposition        = {0.f, 0.f},
                                                const sf::Vector2f& idir             = {1.f, 0.f},
                                                float               iscalar_velocity = 0.f,
                                                float               imass            = 1.f,
                                                float               ielasticity      = 0.5f) {
        return std::make_shared<physic_group>(
            iposition, idir, iscalar_velocity, imass, ielasticity);
    }

    void append(std::shared_ptr<physic_point> physic_element) {
        physic_element->direction(_dir);
        physic_element->scalar_velocity(_scalar_velocity);
        physic_element->mass(_mass);
        physic_element->elasticity(_elasticity);

        auto tmp_pos = physic_element->get_position();

        physic_element->position(tmp_pos + _position);
        _elements.emplace_back(std::move(physic_element), tmp_pos);
        _elements.back().first->_group = weak_from_this();
    }

    void update_bb(float timestep) override {
        for (auto& [e, _] : _elements)
            e->update_bb(timestep);

        physic_point::record_dir_and_velocity();
    }

    void move(float timestep) override {
        if (_elements.empty())
            return;

        _position += get_velocity() * timestep;
        for (auto& [e, displ] : _elements)
            e->position(_position + displ);
    }

private:
    using element_t = std::pair<std::shared_ptr<physic_point>, sf::Vector2f>;
    std::vector<element_t> _elements;

public:
    void user_data(u64 value) override {
        physic_point::user_data(value);
        for (auto& [e, _] : _elements)
            e->user_data(value);
    }

    void user_any(std::any value) override {
        physic_point::user_any(value);
        for (auto& [e, _] : _elements)
            e->user_any(value);
    }

    void record_dir_and_velocity() override {
        physic_point::record_dir_and_velocity();
        for (auto& [e, displ] : _elements)
            e->record_dir_and_velocity();
    }

    void position(const sf::Vector2f& value) override {
        physic_point::position(value);
        for (auto& [e, displ] : _elements)
            e->position(_position + displ);
    }

    void direction(const sf::Vector2f& value) override {
        physic_point::direction(value);
        for (auto& [e, _] : _elements)
            e->direction(value);
    }

    void scalar_velocity(float value) override {
        physic_point::scalar_velocity(value);
        for (auto& [e, _] : _elements)
            e->scalar_velocity(value);
    }

    void velocity(const sf::Vector2f& value) override {
        physic_point::velocity(value);
        for (auto& [e, _] : _elements)
            e->velocity(value);
    }

    void scalar_impulse(float value) override {
        physic_point::scalar_impulse(value);
        for (auto& [e, _] : _elements)
            e->scalar_impulse(value);
    }

    void impulse(const sf::Vector2f& value) override {
        physic_point::impulse(value);
        for (auto& [e, _] : _elements)
            e->impulse(value);
    }

    void mass(float value) override {
        physic_point::mass(value);
        for (auto& [e, _] : _elements)
            e->mass(value);
    }

    void elasticity(float value) override {
        physic_point::elasticity(value);
        for (auto& [e, _] : _elements)
            e->elasticity(value);
    }

    void enable_gravity(bool value = true) override {
        physic_point::enable_gravity(value);
        for (auto& [e, _] : _elements)
            e->enable_gravity(value);
    }

    [[nodiscard]]
    bounding_box pos_bb() const override {
        auto b = bounding_box::maximized();
        for (auto& [e, _] : _elements)
            b.merge(e->pos_bb());
        return b;
    }

    [[nodiscard]]
    bool line_only() const override {
        for (auto& [e, _] : _elements) {
            if (!e->line_only())
                return false;
        }
        return true;
    }
};


class group_tree_iterator {
public:
    group_tree_iterator(physic_point* start = nullptr): p(start) {
        while (auto grp = dynamic_cast<physic_group*>(p)) {
            g = grp;
            if (g->_elements.empty()) {
                //p = nullptr;
                //break;
                throw std::runtime_error("Empty group");
            }

            p = g->_elements.front().first.get();
            idxs.push_back(0);
        }
    }

    group_tree_iterator& operator++() {
        if (!idxs.empty()) {
            ++idxs.back();

            while (!idxs.empty() && idxs.back() == g->_elements.size()) {
                idxs.pop_back();
                if (idxs.empty()) {
                    p = nullptr;
                    break;
                } else {
                    ++idxs.back();
                    g = g->_group.lock().get();
                }
            }

            if (!idxs.empty()) {
                p = g->_elements[idxs.back()].first.get();
                while (auto grp = dynamic_cast<physic_group*>(p)) {
                    g = grp;
                    if (g->_elements.empty()) {
                        //p = nullptr;
                        //break;
                        throw std::runtime_error("Empty group");
                    }

                    p = g->_elements.front().first.get();
                    idxs.push_back(0);
                }
            }
        } else {
            p = nullptr;
        }
        return *this;
    }

    group_tree_iterator operator++(int) {
        auto res = *this;
        ++(*this);
        return res;
    }

    bool operator==(const group_tree_iterator& rhs) const {
        return p == rhs.p;
    }

    physic_point* operator*() {
        return p;
    }

    physic_point* operator->() {
        return p;
    }

private:
    physic_point* p;
    std::vector<u32> idxs;
    physic_group* g = nullptr;
};

class group_tree_view {
public:
    group_tree_view(physic_point* p): _b(p) {}

    [[nodiscard]]
    auto begin() const {
        return _b;
    }

    [[nodiscard]]
    auto end() const {
        return _e;
    }

private:
    group_tree_iterator _b, _e;
};

}
