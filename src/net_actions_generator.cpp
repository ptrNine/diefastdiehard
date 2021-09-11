#include "net_actions.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <map>

int main() {
    std::ios::sync_with_stdio(false);

    std::string_view serialize_suf = "void serialize(auto& _s) const; void deserialize(auto& _d); ";

    std::string line;

    struct struct_params {
        std::vector<std::string> base_classes;
        std::vector<std::string> fields;
    };

    std::map<std::string, struct_params> structs;

    size_t pos;

    while (std::getline(std::cin, line)) {
        /*
        if ((pos = line.find(action_suf)) != std::string::npos) {
            if ((pos = line.find(" : net_spec")) != std::string::npos) {
                std::vector<std::string> bases = {"net_spec"};

                auto name_end   = pos;
                auto name_start = name_end - 1;
                while (line[name_start] != ' ' && line[name_start] != '\t') --name_start;
                ++name_start;

                auto name =
                    std::string(line.begin() + int(name_start), line.begin() + int(name_end));

                pos += sizeof(" : net_spec") - 1;
                while (line[pos] == ' ') ++pos;
                if (line[pos] == ',') {
                    ++pos;
                    while (line[pos] == ' ') ++pos;
                    std::string base;
                    while (line[pos] != ' ' && line[pos] != '\t' && line[pos] != '{')
                        base.push_back(line[pos++]);

                    bases.push_back(std::move(base));
                }

                actions.push_back({std::move(name), std::move(bases)});
            }
        }
        */
        if ((pos = line.find(serialize_suf)) != std::string::npos) {
            std::string_view name = line;
            if (name.starts_with("struct"))
                name.remove_prefix(sizeof("struct") - 1);

            while (!name.empty() && (name.front() == ' ' || name.front() == '\t'))
                name = name.substr(1);

            size_t name_end = 0;
            while ((name[name_end] >= 'A' && name[name_end] <= 'Z') ||
                   (name[name_end] >= 'a' && name[name_end] <= 'z') ||
                   (name[name_end] >= '0' && name[name_end] <= '9') ||
                   name[name_end] == '_')
                ++name_end;

            auto line_v = name.substr(name_end);
            name.remove_suffix(name.size() - name_end);

            while (!line_v.empty() && (line_v.front() == ' ' || line_v.front() == '\t'))
                line_v.remove_prefix(1);

            auto& [bases, fields] = structs[std::string(name)];

            if (line_v.starts_with(':')) {
                line_v.remove_prefix(1);

                std::string base;
                bool        on_tmpl = false;
                while (!line_v.empty() && line_v.front() != '{') {
                    switch (line_v.front()) {
                    case ',':
                        if (!on_tmpl) {
                            bases.push_back(base);
                            base.clear();
                        }
                        break;
                    case ' ':
                    case '\t': break;
                    case '<':
                        on_tmpl = true;
                        base.push_back(line_v.front());
                        break;
                    case '>': on_tmpl = false;
                    default: base.push_back(line_v.front());
                    }
                    line_v.remove_prefix(1);
                }
                if (!base.empty())
                    bases.push_back(base);
            }

            line_v = std::string_view(line).substr(pos + serialize_suf.size());
            size_t fields_pos = 0;
            std::string field_name;

            bool space_was = false;
            bool on_eq = false;
            while (line_v[fields_pos] != '}') {
                auto c = line_v[fields_pos];
                if (c == ' ' || c == '\t')
                    space_was = true;
                else if (c == '=') {
                    on_eq = true;
                }
                else if (c == ';' && !field_name.empty()) {
                    on_eq = false;
                    fields.push_back(field_name);
                    field_name.clear();
                }
                else if (!on_eq) {
                    if (space_was) {
                        field_name.clear();
                        space_was = false;
                    }
                    field_name.push_back(c);
                }
                ++fields_pos;
            }
            if (!field_name.empty())
                fields.push_back(field_name);
        }
    }

    if (structs.empty())
        return 1;

    std::cout <<
        "/* Generated with net_actions_generator.cpp\n"
        " * Command: g++ -E src/net_actions.hpp | net_actions_generator > src/net_action_generated.hpp\n"
        " */\n"
        "\n"
        "#pragma once\n"
        "\n"
        "#include <stdexcept>\n"
        "#include \"net_actions.hpp\"\n"
        "#include \"print.hpp\"\n"
        "#include \"hash_functions.hpp\"\n"
        "\n"
        "namespace dfdh {\n"
        "\n"
        "class unknown_net_action : public std::invalid_argument {\n"
        "public:\n"
        "    unknown_net_action(u32 act): std::invalid_argument(\"Unknown action with id \" + std::to_string(act)) {}\n"
        "};\n"
        "\n"
        "inline bool net_action_dispatch(const address_t& address, const packet_t& packet, auto&& overloaded) {\n"
        "    auto act = packet.cast_to<u32>();\n"
        "    switch (act) {\n";

    for (auto& [struct_name, _] : structs) {
        if (struct_name.starts_with("a_"))
            std::cout <<
                "    case " << struct_name << "::ACTION:\n"
                "        if constexpr (std::is_invocable_v<decltype(overloaded), " << struct_name << ">) {\n"
                "            overloaded(packet.cast_to<" << struct_name << ">());\n"
                "            return true;\n"
                "        }\n"
                "        else if constexpr (std::is_invocable_v<decltype(overloaded), address_t, " << struct_name << ">) {\n"
                "            overloaded(address, packet.cast_to<" << struct_name << ">());\n"
                "            return true;\n"
                "        }\n"
                "        break;\n";
    }
    std::cout <<
        "    default:\n"
        "        throw unknown_net_action(act);\n"
        "    }\n"
        "    return false;\n"
        "}\n"
        "\n"
        "\n"
        "inline bool net_action_downcast(const net_spec& action_base, auto&& overloaded) {\n"
        "    switch (action_base.action) {\n";
    for (auto& [struct_name, _] : structs)
        if (struct_name.starts_with("a_"))
            std::cout <<
                "    case " << struct_name << "::ACTION:\n"
                "        if constexpr (std::is_invocable_v<decltype(overloaded), " << struct_name << ">) {\n"
                "            overloaded(static_cast<const " << struct_name << "&>(action_base)); // NOLINT\n"
                "            return true;\n"
                "        }\n"
                "        break;\n";
    std::cout <<
        "    default:\n"
        "        throw unknown_net_action(action_base.action);\n"
        "    }\n"
        "    return false;\n"
        "}\n"
        "\n"
        "\n"
        "inline bool net_action_downcast(const address_t& address, const net_spec& action_base, auto&& overloaded) {\n"
        "    switch (action_base.action) {\n";
    for (auto& [struct_name, _] : structs)
        if (struct_name.starts_with("a_"))
            std::cout <<
                "    case " << struct_name << "::ACTION:\n"
                "        if constexpr (std::is_invocable_v<decltype(overloaded), address_t, " << struct_name << ">) {\n"
                "            overloaded(address, static_cast<const " << struct_name << "&>(action_base)); // NOLINT\n"
                "            return true;\n"
                "        }\n"
                "        break;\n";
    std::cout <<
        "    default:\n"
        "        throw unknown_net_action(action_base.action);\n"
        "    }\n"
        "    return false;\n"
        "}\n"
        "\n"
        "\n"
        "#define net_action_object_dispatch(OBJECT, member_function, ADDRESS, PACKET) \\\n"
        "    [](auto& object, const address_t& address, packet_t& packet) { \\\n"
        "        auto act = packet.cast_to<u32>(); \\\n"
        "        switch (act) { \\\n";
    for (auto& [struct_name, _] : structs) {
        if (struct_name.starts_with("a_"))
            std::cout <<
                "        case " << struct_name << "::ACTION: \\\n"
                "            if constexpr (requires{object.member_function(" << struct_name << "());}) { \\\n"
                "                object.member_function(packet.cast_to<" << struct_name << ">()); \\\n"
                "                return true; \\\n"
                "            } \\\n"
                "            else if constexpr (requires{object.member_function(address_t(), " << struct_name << "());}) { \\\n"
                "                object.member_function(address, packet.cast_to<" << struct_name << ">()); \\\n"
                "                return true; \\\n"
                "            } \\\n"
                "            break; \\\n";
    }
    std::cout <<
        "        default: \\\n"
        "            throw unknown_net_action(act); \\\n"
        "        } \\\n"
        "        return false; \\\n"
        "    }(OBJECT, ADDRESS, PACKET)\n"
        "\n"
        "\n"
        "inline void action_serialize_setup_hash(auto& _s) {\n"
        "    auto hash = fnv1a64(_s.data() + sizeof(net_spec), _s.size() - sizeof(net_spec));\n"
        "    if constexpr (std::endian::native == std::endian::big)\n"
        "        hash = bswap(hash);\n"
        "    constexpr auto hash_pos = sizeof(net_spec) - sizeof(u64);\n"
        "    ::memcpy(_s.data() + hash_pos, &hash, sizeof(hash));\n"
        "}\n"
        "\n";

    for (auto& [struct_name, params] : structs) {
        std::cout <<
            "inline void " << struct_name << "::serialize(auto& _s) const {\n";
        for (auto& base : params.base_classes)
            std::cout <<
                "    " << base << "::serialize(_s);\n";
        if (!params.fields.empty()) {
            std::cout << "    serialize_all(_s";
            for (auto& field : params.fields)
                std::cout << ", " << field;
            std::cout << ");\n";
        }
        if (struct_name.starts_with("a_") && (!params.fields.empty() || params.base_classes.size() > 1))
            std::cout << "    action_serialize_setup_hash(_s);\n";
        std::cout <<
            "}\n"
            "inline void " << struct_name << "::deserialize(auto& _d) {\n";
        for (auto& base : params.base_classes)
            std::cout <<
                "    " << base << "::deserialize(_d);\n";
        if (!params.fields.empty()) {
            std::cout << "    deserialize_all(_d";
            for (auto& field : params.fields)
                std::cout << ", " << field;
            std::cout << ");\n";
        }
        std::cout <<
            "}\n"
            "template <>\n"
            "struct printer<" << struct_name << "> {\n"
            "    void operator()(std::ostream& os, const " << struct_name << "& act) const {\n";
        for (auto& base : params.base_classes)
            std::cout <<
                "        fprint(os, static_cast<const " << base << "&>(act));\n";
        if (!params.fields.empty()) {
            std::cout <<
                "        fprint(os,\n";
            for (auto& field : params.fields)
                std::cout <<
                    "               \"  " << field << ": \", act." << field << ",\n";
            std::cout <<
                "               \"\");\n";
        }
        std::cout <<
            "    }\n"
            "};\n"
            "\n";
    }

    std::cout <<
        "} // namespace dfdh\n";
}
