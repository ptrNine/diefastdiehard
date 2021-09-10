#include "net_actions.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <map>

int main() {
    std::ios::sync_with_stdio(false);

    std::string_view start_counter_pref = "static constexpr inline u32 _DFDH_START_COUNTER = ";
    std::string_view end_counter_pref = "static constexpr inline u32 _DFDH_END_COUNTER = ";
    std::string_view action_number_suf = " : net_spec { static constexpr inline u32 ACTION =";
    std::string_view serialize_suf = "void serialize(auto& _s) const; void deserialize(auto& _d); ";

    std::string line;

    std::optional<uint32_t> start_counter;
    std::optional<uint32_t> end_counter;
    std::vector<std::pair<std::string, uint32_t>> action_counters;
    std::map<std::string, std::vector<std::string>> serialize_fields;

    size_t pos;

    while (std::getline(std::cin, line)) {
        if (!start_counter && (pos = line.find(start_counter_pref)) != std::string::npos) {
            pos += start_counter_pref.size();
            uint32_t c = 0;
            while (line[pos] >= '0' && line[pos] <= '9') c = c * 10 + uint32_t(line[pos++] - '0');
            start_counter = c;
        }
        if (!end_counter && (pos = line.find(end_counter_pref)) != std::string::npos) {
            pos += end_counter_pref.size();
            uint32_t c = 0;
            while (line[pos] >= '0' && line[pos] <= '9') c = c * 10 + uint32_t(line[pos++] - '0');
            end_counter = c;
        }
        if ((pos = line.find(action_number_suf)) != std::string::npos) {
            auto     name_end = pos;
            uint32_t c        = 0;
            while (line[pos] >= '0' && line[pos] <= '9') c = c * 10 + uint32_t(line[pos++] - '0');

            auto name_start = name_end - 1;
            while (line[name_start] != ' ' && line[name_start] != '\t') --name_start;
            ++name_start;

            auto name = std::string(line.begin() + int(name_start), line.begin() + int(name_end));
            action_counters.push_back({name, c});
        }
        if ((pos = line.find(serialize_suf)) != std::string::npos) {
            std::string_view name = line;
            if (name.starts_with("struct "))
                name.remove_prefix(sizeof("struct ") - 1);

            size_t name_end = 0;
            while ((name[name_end] >= 'A' && name[name_end] <= 'Z') ||
                   (name[name_end] >= 'a' && name[name_end] <= 'z') ||
                   (name[name_end] >= '0' && name[name_end] <= '9') ||
                   name[name_end] == '_')
                ++name_end;
            name.remove_suffix(name.size() - name_end);

            auto& fields = serialize_fields[std::string(name)];
            auto fields_pos = pos + serialize_suf.size();
            std::string field_name;

            while (line[fields_pos] != '}') {
                auto c = line[fields_pos];
                if (c == ' ' || c == '\t')
                    field_name.clear();
                else if (c == ';' && !field_name.empty()) {
                    fields.push_back(field_name);
                    field_name.clear();
                }
                else
                    field_name.push_back(c);
                ++fields_pos;
            }
            if (!field_name.empty())
                fields.push_back(field_name);
        }
    }

    if (!start_counter || !end_counter || action_counters.empty())
        return 1;

    for (auto& [act_name, counter] : action_counters)
        counter -= *start_counter + 1;

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

    for (auto& [act_name, counter] : action_counters) {
        std::cout <<
            "    case " << act_name << "::ACTION:\n"
            "        if constexpr (std::is_invocable_v<decltype(overloaded), " << act_name << ">) {\n"
            "            overloaded(packet.cast_to<" << act_name << ">());\n"
            "            return true;\n"
            "        }\n"
            "        else if constexpr (std::is_invocable_v<decltype(overloaded), address_t, " << act_name << ">) {\n"
            "            overloaded(address, packet.cast_to<" << act_name << ">());\n"
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
    for (auto& [act_name, counter] : action_counters)
        std::cout <<
            "    case " << act_name << "::ACTION:\n"
            "        if constexpr (std::is_invocable_v<decltype(overloaded), " << act_name << ">) {\n"
            "            overloaded(static_cast<const " << act_name << "&>(action_base)); // NOLINT\n"
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
    for (auto& [act_name, counter] : action_counters)
        std::cout <<
            "    case " << act_name << "::ACTION:\n"
            "        if constexpr (std::is_invocable_v<decltype(overloaded), address_t, " << act_name << ">) {\n"
            "            overloaded(address, static_cast<const " << act_name << "&>(action_base)); // NOLINT\n"
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
    for (auto& [act_name, counter] : action_counters) {
        std::cout <<
        "        case " << act_name << "::ACTION: \\\n"
        "            if constexpr (requires{object.member_function(" << act_name << "());}) { \\\n"
        "                object.member_function(packet.cast_to<" << act_name << ">()); \\\n"
        "                return true; \\\n"
        "            } \\\n"
        "            else if constexpr (requires{object.member_function(address_t(), " << act_name << "());}) { \\\n"
        "                object.member_function(address, packet.cast_to<" << act_name << ">()); \\\n"
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

    for (auto& [action, fields] : serialize_fields) {
        std::cout <<
            "inline void " << action << "::serialize(auto& _s) const {\n"
            "    net_spec::serialize(_s);\n";
        if (!fields.empty()) {
            std::cout << "    serialize_all(_s";
            for (auto& field : fields)
                std::cout << ", " << field;
            std::cout << ");\n";
            if (!fields.empty())
                std::cout << "    action_serialize_setup_hash(_s);\n";
        }
        std::cout <<
            "}\n"
            "inline void " << action << "::deserialize(auto& _d) {\n"
            "    net_spec::deserialize(_d);\n";
        if (!fields.empty()) {
            std::cout << "    deserialize_all(_d";
            for (auto& field : fields)
                std::cout << ", " << field;
            std::cout << ");\n";
        }
        std::cout <<
            "}\n"
            "template <>\n"
            "struct printer<" << action << "> {\n"
            "    void operator()(std::ostream& os, const " << action << "& act) const {\n"
            "        os << \"action: \" << act.action\n"
            "           << \"  transcontrol: \" << act.transcontrol\n"
            "           << \"  id: \" << act.id\n"
            "           << \"  hash: \" << act.hash\n";
        for (auto& field : fields)
            std::cout <<
                "           << \"  " << field << ": \" << act." << field << "\n";
        std::cout <<
            "           << \"\";\n"
            "    }\n"
            "};\n"
            "\n";
    }

    std::cout <<
        "} // namespace dfdh\n";
}
