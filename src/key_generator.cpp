#include <iostream>
#include <string>
#include <vector>

static std::string_view trim_spaces(std::string_view str) {
    while (!str.empty() && (str.front() == ' ' || str.front() == '\t')) str = str.substr(1);
    while (!str.empty() && (str.back() == ' ' || str.back() == '\t'))
        str = str.substr(0, str.size() - 1);
    return str;
}

int main() {
    std::vector<std::string> keys;
    std::string line;
    bool on_enum = false;

    while (std::getline(std::cin, line)) {
        auto l = trim_spaces(line);

        if (!on_enum) {
            if (l.starts_with("enum Key"))
                on_enum = true;
            continue;
        }

        if (l.empty() || l == "{" || l.starts_with("Unknown"))
            continue;

        if (l == "A = 0,")
            keys.push_back("A");
        else if (l.starts_with("KeyCount"))
            break;
        else {
            auto f = l.find(',');
            if (f != std::string_view::npos) {
                keys.push_back(std::string(l.substr(0, f)));
            }
        }
    }

    std::cout <<
        "#pragma once\n"
        "\n"
        "#include <map>\n"
        "#include <string>\n"
        "#include <SFML/Window/Keyboard.hpp>\n"
        "\n"
        "namespace dfdh {\n"
        "\n"
        "inline std::string sfml_key_to_str(sf::Keyboard::Key key) {\n"
        "    switch (key) {\n";
    for (auto& key : keys)
        std::cout <<
            "        case sf::Keyboard::" << key << ": return \"" << key << "\";\n";
    std::cout <<
        "        default: return {};\n"
        "    }\n"
        "}\n"
        "inline sf::Keyboard::Key sfml_str_to_key(std::string_view str) {\n"
        "   static constexpr auto init_f = [] {\n"
        "       return std::map<std::string_view, sf::Keyboard::Key>{\n";
    for (auto& key : keys)
        std::cout <<
            "           {\"" << key << "\", sf::Keyboard::" << key << "},\n";
    std::cout <<
        "       };\n"
        "   };\n"
        "   static auto map = init_f();\n"
        "   auto found = map.find(str);\n"
        "   if (found != map.end())\n"
        "       return found->second;\n"
        "   return sf::Keyboard::Unknown;\n"
        "}\n"
        "} // namespace dfdh\n";
}
