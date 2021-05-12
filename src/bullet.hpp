#pragma once

#include <SFML/Graphics/RenderWindow.hpp>
#include <filesystem>
#include <list>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>

#include "physic_simulation.hpp"
#include "types.hpp"

namespace dfdh {

class bullet_sprite_cache {
public:
    static constexpr float bullet_x = 150.f;
    static constexpr float bullet_y = 15.f;

    static bullet_sprite_cache& instance() {
        static bullet_sprite_cache inst;
        return inst;
    }

    bullet_sprite_cache(const bullet_sprite_cache&) = delete;
    bullet_sprite_cache& operator=(const bullet_sprite_cache&) = delete;

    sf::Sprite& sprite() {
        return _sprite;
    }

    [[nodiscard]]
    sf::Vector2f scale_f() const {
        return {_xf, _yf};
    }

private:
    bullet_sprite_cache() {
        _txtr.loadFromFile(std::filesystem::current_path() / "data/textures/bullet.png");
        _txtr.setSmooth(true);
        _sprite.setTexture(_txtr);
        auto sz = _txtr.getSize();
        _sprite.setOrigin(float(sz.x), float(sz.y) / 2.f);

        _xf = bullet_x / float(sz.x);
        _yf = bullet_y / float(sz.y);
        _sprite.setScale(_xf, _yf);
    }

    ~bullet_sprite_cache() = default;

private:
    sf::Sprite _sprite;
    sf::Texture _txtr;
    float       _xf;
    float       _yf;
};


inline bullet_sprite_cache& bullet_sprite() {
    return bullet_sprite_cache::instance();
}

class bullet {
public:
    bullet(physic_simulation&  sim,
           const sf::Vector2f& position,
           float               mass,
           const sf::Vector2f& velocity) {
        _ph = physic_point::create(position, normalize(velocity), magnitude(velocity), mass);
        sim.add_primitive(_ph);
    }

    bullet(const bullet&) = delete;
    bullet& operator=(const bullet&) = delete;
    bullet(bullet&&) = delete;
    bullet& operator=(bullet&&) = delete;

    [[nodiscard]]
    auto& physic() {
        return _ph;
    }

private:
    std::shared_ptr<physic_point> _ph;
};

class bullet_mgr {
public:
    template <typename F>
    bullet_mgr(const std::string& name, physic_simulation& sim, F hit_callback): _name("bm_" + name) {
        sim.add_collide_callback(_name, std::move(hit_callback));
    }

    void shot(physic_simulation& sim, const sf::Vector2f& position, float mass, const sf::Vector2f& velocity) {
        _bullets.emplace_back(sim, position, mass, velocity);
        auto& blt = _bullets.back();
        blt.physic()->user_data(0xdeadbeef);
    }

    void draw(sf::RenderWindow& wnd) {
        for (auto i = _bullets.begin(); i != _bullets.end();) {
            if (i->physic()->ready_delete_later()) {
                _bullets.erase(i++);
                continue;
            }

            if (i->physic()->get_distance() > 15000.f)
                i->physic()->delete_later();

            auto pos = i->physic()->get_position();
            auto dir = i->physic()->get_direction();
            auto angle = std::atan2(dir.y, dir.x);

            auto x_sz = std::min(i->physic()->get_distance(), bullet_sprite_cache::bullet_x);
            auto xf = (x_sz / bullet_sprite_cache::bullet_x) * bullet_sprite().scale_f().x;
            bullet_sprite().sprite().setScale(xf, bullet_sprite().scale_f().y);
            bullet_sprite().sprite().setPosition(pos);
            bullet_sprite().sprite().setRotation(angle * 180.f / M_PIf32);
            wnd.draw(bullet_sprite().sprite());

            ++i;
        }
    }

private:
    std::string       _name;
    std::list<bullet> _bullets;
};
}
