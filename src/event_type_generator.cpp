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
    std::vector<std::string> event_types;
    std::string line;
    bool on_enum = false;

    while (std::getline(std::cin, line)) {
        auto l = trim_spaces(line);

        if (!on_enum) {
            if (l.starts_with("enum EventType"))
                on_enum = true;
            continue;
        }

        if (l.empty() || l == "{")
            continue;

        if (l.starts_with("Count"))
            break;
        else {
            auto f = l.find(',');
            if (f != std::string_view::npos) {
                event_types.push_back(std::string(l.substr(0, f)));
            }
        }
    }

    std::cout <<
        "#pragma once\n"
        "\n"
        "#include <string>\n"
        "#include <map>\n"
        "#include <SFML/Window/Event.hpp>\n"
        "#include \"log.hpp\"\n"
        "\n"
        "namespace dfdh {\n"
        "\n"
        "inline std::string sfml_event_type_to_str(sf::Event::EventType event_type) {\n"
        "    switch (event_type) {\n";
    for (auto& event : event_types)
        std::cout <<
            "        case sf::Event::" << event << ": return \"" << event << "\";\n";
    std::cout <<
        "        default: return {};\n"
        "    }\n"
        "}\n"
        "inline sf::Event::EventType sfml_str_to_event_type(std::string_view str) {\n"
        "    static auto map = []{\n"
        "        return std::map<std::string_view, sf::Event::EventType>{\n";
    for (auto& event: event_types)
        std::cout <<
        "        {\"" << event << "\", sf::Event::" << event << "},\n";
        std::cout << "    };\n"
            "    }();\n"
            "    auto found = map.find(str);\n"
            "    if (found != map.end())\n"
            "        return found->second;\n"
            "    LOG_ERR(\"invalid sfml event type: {}\", str);\n"
            "    return sf::Event::EventType(0);\n"
            "}\n"
            "} // namespace dfdh\n";
}
