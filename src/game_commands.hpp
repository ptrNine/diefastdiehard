#pragma once

#include "base/ston.hpp"
#include "game_state.hpp"
#include "command_buffer.hpp"

namespace dfdh {

class game_commands {
public:
    game_commands(game_state& state): gs(state) {
        command_buffer().add_handler("player", &game_commands::cmd_player, this);
        command_buffer().add_handler("player delete", &game_commands::cmd_player_delete, this);
        command_buffer().add_handler("player wpn", &game_commands::cmd_player_wpn, this);
        command_buffer().add_handler("cfg", &game_commands::cmd_cfg, this);
        command_buffer().add_handler("cfg set", &game_commands::cmd_cfg_set, this);
        command_buffer().add_handler("cfg mode", &game_commands::cmd_cfg_mode, this);
        command_buffer().add_handler("cfg watch", &game_commands::cmd_cfg_watch, this);
        command_buffer().add_handler("cfg watch remove", &game_commands::cmd_cfg_watch_remove, this);
        command_buffer().add_handler("cfg watch list", &game_commands::cmd_cfg_watch_list, this);
        command_buffer().add_handler("cfg reload", &game_commands::cmd_cfg_reload, this);
        command_buffer().add_handler("cfg list", &game_commands::cmd_cfg_list, this);
        command_buffer().add_handler("cfg control", &game_commands::cmd_cfg_control, this);
        command_buffer().add_handler("help", &game_commands::cmd_help, this);
        command_buffer().add_handler("list", &game_commands::cmd_list, this);
        command_buffer().add_handler("game", &game_commands::cmd_game, this);
        command_buffer().add_handler("game speed", &game_commands::cmd_game_speed, this);
        command_buffer().add_handler("level", &game_commands::cmd_level, this);
        command_buffer().add_handler("ai", &game_commands::cmd_ai, this);
        command_buffer().add_handler("ai difficulty", &game_commands::cmd_ai_difficulty, this);
        command_buffer().add_handler("ai profiler", &game_commands::cmd_ai_profiler, this);
        command_buffer().add_handler("shutdown", &game_commands::cmd_shutdown, this);
        command_buffer().add_handler("sound volume", &game_commands::cmd_sound_volume, this);

        if (gs.lua_cmd_enabled)
            command_buffer().add_handler("lua", &game_commands::cmd_lua, this);
    }

    ~game_commands() {
        command_buffer().remove_handler("player");
        command_buffer().remove_handler("cfg");
        command_buffer().remove_handler("help");
        command_buffer().remove_handler("list");
        command_buffer().remove_handler("game");
        command_buffer().remove_handler("level");
        command_buffer().remove_handler("ai");
        command_buffer().remove_handler("connect");
        command_buffer().remove_handler("srv init");
        command_buffer().remove_handler("shutdown");
    }

    void cmd_help(const std::string& cmd) {
        std::string_view help;
        if (cmd == "player") {
            help = "available commands:\n"
                   "  player delete [player_name]                - delete player\n"
                   "  player reload [player_name]?               - reload player(s) configuration\n"
                   "  player controller[N] [player_name]         - binds specified controller to player\n"
                   "  player controller[N] delete                - deletes specified controller\n"
                   "  player conf [player_name]                  - open configuration ui for player\n"
                   "  player wpn [player_name] [wpn_section]?    - set/or show player weapon\n"
                   "  player create [player_name|player_params]  - create player\n"
                   "      player_params must be set in \"name=NAME group=INT\" format";
        }
        else if (cmd == "log") {
            help = "available commands:\n"
                   "  log time [on/off]  - enables or disables time showing\n"
                   "  log level [on/off] - enables or disables level showing\n"
                   "  log ring  [uint]   - setup log ring buffer size\n"
                   "  log clear          - clear devconsole log";
        }
        else if (cmd == "cfg") {
            help = "shows loaded configs\n"
                   "available commands:\n"
                   "  cfg [section_name] [key]?                    - show key/values for specified section\n"
                   "  cfg reload [weapons|levels]?                 - reload config users\n"
                   "  cfg list [prefix]?                           - show sections\n"
                   "  cfg set [section_name] [key] [value]?        - setup value for specified section\n"
                   "  cfg mode [readonly/readwrite] | [r/rw]       - setup config mode\n"
                   "  cfg watch [section_name]                     - watch section for changes and automatically\n"
                   "                                                 reload dependent entities\n"
                   "  cfg watch remove [section_name]              - remove section from watch list\n"
                   "  cfg watch list                               - list watched sections\n"
                   "  cfg control [section_name] [key] [dx] [dy]?  - enable control for config value";
        }
        else if (cmd == "level") {
            help = "level manipulations\n"
                   "available commands:\n"
                   "  level list cached?            - list available (or cached) levels\n"
                   "  level cache [level_section]   - cache specified level\n"
                   "  level cache clear             - clear levels cache\n"
                   "  level current                 - prints section name of the current level\n"
                   "  level current [level_section] - setup current level\n"
                   "  level current none            - reset current level";
        }
        else if (cmd == "ai") {
            help = "ai operator settings\n"
                   "available commands:\n"
                   "  ai list                                    - lists all operated player names\n"
                   "  ai bind [player_name]                      - binds player to the ai operator\n"
                   "  ai difficulty [player_name] [difficulty]?  - shows or setups ai difficulty\n"
                   "    difficulty: easy medium hard\n"
                   "  ai profiler [on/off]                       - enable/disable ai profiler";
        }
        else if (gs.lua_cmd_enabled && cmd == "lua") {
            help = "Lua interpreter\n"
                   "lua [command]    - execute lua line";
        }
        else if (cmd == "sound") {
            help = "Sound settings\n"
                   "sound volume [0 - 100]  - set or get sound volume";
        }

        if (!help.empty())
            LOG("{}", help);
    }

    void cmd_player_delete(const std::string& player_name) {
        auto found = gs.players.find(player_name);
        if (found != gs.players.end()) {
            auto found_ai = gs.ai_operators.find(player_name);
            if (found_ai != gs.ai_operators.end()) {
                gs.ai_operators.erase(found_ai);
                LOG_INFO("ai operator {} deleted", player_name);
            }

            gs.players.erase(found);
            LOG_INFO("player {} deleted", player_name);
        }
        else {
            LOG_ERR("player {} not found", player_name);
        }
    }

    void cmd_player_wpn(const std::string& player_name, const std::optional<std::string>& wpn_section) {
        auto found = gs.players.find(player_name);
        if (found == gs.players.end()) {
            LOG_ERR("player {} not found", player_name);
            return;
        }

        auto& player = *found->second;
        if (wpn_section) {
            if (weapon_mgr().is_exists(*wpn_section))
                player.setup_pistol(*wpn_section);
            else
                LOG_ERR("weapon {} not found", *wpn_section);
        }
        else {
            LOG_INFO("player wpn {}: {}", player_name, player.pistol_section());
        }
    }

    void cmd_lua(const std::string& cmd) {
        gs.lua_schedule_execution(cmd);
    }

    void cmd_player(const std::string& cmd, const std::optional<std::string>& value) {
        if (cmd == "reload") {
            gs.player_conf_reload(value ? player_name_t(*value) : player_name_t{});
        }
        else if (cmd.starts_with("controller")) {
            auto strnum = cmd.substr("controller"sv.size());
            if (strnum.empty()) {
                LOG_ERR("player controller: missing controller number");
                return;
            }

            u32 controller_id;
            try {
                controller_id = ston<u32>(strnum);
            } catch (...) {
                LOG_ERR("player controller: invalid controller number");
                return;
            }

            if (!value) {
                LOG_ERR("player {}: missing action or player name", cmd);
                return;
            }

            auto ok = *value == "delete" ?
                gs.controller_delete(controller_id) :
                gs.controller_bind(controller_id, *value);

            if (ok)
                gs.rebuild_controlled_players();
        }
        else if (cmd == "create") {
            if (!value) {
                LOG_ERR("player create: missing player name argument");
                return;
            }

            auto        params = *value / split(' ', '\t');
            std::string name;
            int         group = 0;
            for (auto param : params) {
                auto prm = std::string_view(param.begin(), param.end());
                if (prm.starts_with("name="))
                    name = std::string(prm.substr("name="sv.size()));
                else if (prm.starts_with("group=")) {
                    auto group_str = std::string(prm.substr("group="sv.size()));
                    try {
                        group = ston<int>(group_str);
                    }
                    catch (...) {
                        LOG_WARN("player create: invalid group number in params");
                    }
                }
                else {
                    name = std::string(prm);
                }
            }
            if (name.empty()) {
                LOG_ERR("player create: missing name in params");
                return;
            }

            if (auto plr = gs.player_create(name))
                plr->group(group);
        }
        else if (cmd == "conf") {
            if (!value) {
                LOG_ERR("player conf: missing player name argument");
                return;
            }

            gs.pconf_ui = std::make_unique<player_configurator_ui>(*value);
            gs.pconf_ui->show(true);
        }
        else if (cmd == "help") {
            cmd_help("player");
        }
        else {
            LOG_ERR("player: unknown subcommand '{}'", cmd);
        }
    }

    void cmd_cfg_list(const std::optional<std::string>& value) {
        std::string sects;
        if (value) {
            for (auto& [sect, _] : cfg::global().get_sections())
                if (sect.starts_with(*value))
                    sects += "\n" + sect;
        }
        else {
            for (auto& [sect, _] : cfg::global().get_sections()) sects += "\n" + sect;
        }
        LOG("Sections:{}", sects);
        return;
    }

    void cmd_cfg_reload(const std::optional<std::string>& type) {

#define reload_type(TYPE, ...)                                                                     \
    do {                                                                                           \
        if (!type || *type == TYPE) {                                                              \
            __VA_ARGS__                                                                            \
            if (type) {                                                                            \
                LOG_INFO(TYPE " configs reloaded!");                                               \
                return;                                                                            \
            }                                                                                      \
        }                                                                                          \
    } while (0)

        reload_type("weapons", weapon_mgr().reload(););
        reload_type("levels",
            for (auto& [_, lvl] : gs.levels) lvl->cfg_reload();
            if (gs.cur_level) {
                gs.cur_level->setup_to(gs.sim);
                gs.events.push(game_state_event::level_changed);
            }
        );

        LOG_INFO("configs reloaded!");
        return;
#undef reload_type
    }

    void cmd_cfg(const std::string& sect, const std::optional<std::string>& value) {
        if (sect == "help") {
            cmd_help("cfg");
            return;
        }

        auto optsect = cfg::global().try_get_section(sect);
        if (!optsect) {
            LOG_ERR("cfg: section [{}] not found", sect);
            return;
        }

        if (value) {
            auto optkey = optsect->try_get<std::string>(*value);
            if (!optkey) {
                LOG_ERR("cfg {}: key '{}' not found", sect, *value);
                return;
            }

            LOG("{} = {}", *value, optkey->value());
        }
        else {
            LOG("{}", *optsect);
            /*
            size_t spaces = 0;
            std::string to_print;
            for (auto& [k, v] : found_sect->second.sects) {
                to_print.push_back('\n');
                to_print += k;
                if (k.size() > spaces)
                    spaces = k.size();
                else
                    to_print.resize(to_print.size() + (spaces - k.size()), ' ');
                to_print += " = "sv;
                to_print += v;
            }
            LOG("{}", to_print);
            */
        }
    }

    void cmd_cfg_set(const std::string& sect, const std::string& key, const std::optional<std::string>& value) {
        auto optsect = cfg::mutable_global().try_get_section(sect);
        if (!optsect) {
            LOG_ERR("cfgset: section [{}] not found", sect);
            return;
        }

        auto optkey = optsect->try_get<std::string>(key);
        if (!optkey) {
            LOG_ERR("cfgset {}: key '{}' not found", sect, key);
            return;
        }

        optkey->set(value ? *value : "");
        cfg::mutable_global().commit();
        LOG_INFO("[{}] {} = {}", sect, key, value ? *value : "");
    }

    void cmd_cfg_mode(const std::optional<std::string>& value) {
        if (value) {
            bool readonly = false;
            if (*value == "readonly" || *value == "r")
                readonly = true;
            else if (*value != "readwrite" && *value != "rw")
                LOG_ERR("cfg mode: invalid argument {} (must be readonly/r or readwrite/rw", *value);
            if (cfg::mutable_global().is_readonly() != readonly) {
                cfg::mutable_global().set_readonly(readonly);
                LOG_INFO("Config has been mounted in {} mode", readonly ? "readonly" : "readwrite");
            }
        }
        else
            LOG_INFO("config mode: {}", cfg::mutable_global().is_readonly() ? "readonly" : "readwrite");
    }

    void cmd_cfg_watch(const std::string& value) {
        try {
            gs.conf_watcher.watch_section(cfg::mutable_global().get_section(value));
            LOG_INFO("watch changes in section [{}]...", value);
        } catch (const std::exception& e) {
            LOG_ERR("cfg watch: {}", e.what());
        }
    }

    void cmd_cfg_watch_list() {
        LOG_INFO("watched sections:");
        for (auto& sect_name : gs.conf_watcher.watched_section_names())
            LOG_INFO("    {}", sect_name);
    }

    void cmd_cfg_watch_remove(const std::string& value) {
        try {
            auto ok = gs.conf_watcher.remove_section(cfg::mutable_global().get_section(value));
            if (ok)
                LOG_INFO("remove section [{}] from watch list", value);
            else
                LOG_ERR("cfg watch remove: section [{}] does not exists", value);
        } catch (const std::exception& e) {
            LOG_ERR("cfg watch remove: {}", e.what());
        }
    }

    void cmd_level(const std::string& cmd, const std::optional<std::string>& value) {
        if (cmd == "list") {
            if (value && *value == "cached") {
                std::vector<std::string> res;
                for (auto& [l, _] : gs.levels)
                    res.push_back(l);
                LOG("Cached levels: {}", res);
            } else {
                LOG("Available levels: {}", level::list_available_levels());
            }
        }
        else if (cmd == "cache") {
            if (value) {
                if (*value == "clear")
                    gs.level_cache_clear();
                else
                    gs.level_cache(*value);
            } else {
                LOG_ERR("log cache: missing level section argument");
            }
        }
        else if (cmd == "current") {
            if (value)
                gs.level_current(*value != "none" ? *value : std::string{});
            else {
                LOG_INFO("level current: {}", gs.cur_level ? gs.cur_level->section_name() : "none");
            }
        }
        else if (cmd == "help") {
            cmd_help("level");
        }
        else {
            LOG_ERR("level: unknown subcommand '{}'", cmd);
        }
    }

    void cmd_game_speed(const std::optional<std::string>& value) {
        if (!value) {
            LOG_INFO("game speed: {}", gs.game_speed);
        }
        else {
            float v;
            try {
                v = ston<float>(*value);
            }
            catch (...) {
                LOG_ERR("game speed: argument must be a number");
                return;
            }
            gs.game_speed = v;
        }
    }

    void cmd_game(const std::optional<std::string>& value) {
        if (value) {
            if (*value == "on")
                gs.game_run();
            else if (*value == "off")
                gs.game_stop();
            else
                LOG_ERR("game: invalid argument {}", *value);
        } else {
            LOG_INFO("game: {}", gs.on_game ? "on" : "off");
        }
    }

    void cmd_ai_difficulty(const std::string& player_name, const std::optional<std::string>& difficulty) {
        auto found = gs.ai_operators.find(player_name);
        if (found == gs.ai_operators.end()) {
            LOG_ERR("ai difficulty: ai operator for player {} was not found", player_name);
            return;
        }

        if (difficulty) {
            try {
                found->second->set_difficulty(*difficulty);
            }
            catch (const std::exception& e) {
                LOG_ERR("ai difficulty: cannot set difficulty {}: {}", *difficulty, e.what());
            }
        }
        else {
            LOG_INFO("ai difficulty {}: {}", player_name, found->second->difficulty());
        }
    }

    void cmd_ai_profiler(bool value) {
        gs.ai_profiler_enabled = value;
    }

    void cmd_ai(const std::string& cmd, const std::optional<std::string>& value) {
        if (cmd == "list") {
            std::string msg = "active AI operators:\n";
            for (auto& [name, _] : gs.ai_operators) msg += "  " + std::string(name);
            LOG("{}", msg);
        }
        else if (cmd == "bind") {
            if (value) {
                if (gs.ai_bind(*value))
                    LOG_INFO("player {} now ai-operated", *value);
            }
            else
                LOG_ERR("ai bind: missing player name");
        }
        else if (cmd == "help") {
            cmd_help("ai");
        }
        else {
            LOG_ERR("ai: unknown subcommand {}", cmd);
        }
    }

    void cmd_cfg_control(const std::string&                section,
                         const std::string&                key,
                         const std::string&                str_steps) {
        auto sect = cfg::global().try_get_section(section);
        if (!sect) {
            LOG_ERR("cfg control: section [{}] was not found", section);
            return;
        }

        if (!sect->has_key(key)) {
            LOG_ERR("cfg control {}: key {} was not found", section, key);
            return;
        }

        cfg_value_control::value_t steps;
        try {
            u32 idx = 0;
            for (auto str_step_view : str_steps / split(' ', '\t')) {
                steps[idx] = ston<float>(std::string(str_step_view.begin(), str_step_view.end()));
                ++idx;
            }
        }
        catch (...) {
            LOG_ERR("cfg control {} {} {}: steps argument must contain numbers only",
                    section,
                    key,
                    str_steps);
            return;
        }

        gs.cfgval_ctrl = cfg_value_control(section, key, steps);
        LOG_INFO("controlling [{}]:{} with steps {}", section, key, str_steps);
    }

    void cmd_list() {
        std::string commands;
        for (auto& [cmd, _] : command_buffer().get_command_tree()->subcmds) commands += "\n" + cmd;
        LOG("available commands: {}", commands);
    }

    void cmd_sound_volume(const std::optional<std::string>& volume) {
        if (volume) {
            try {
                sound_mgr().volume(ston<float>(*volume));
            } catch (...) {
                LOG_ERR("sound volume: value must be a number [0 - 100]");
            }
        }
        else {
            LOG_INFO("sound volume: {}", sound_mgr().volume());
        }
    }

    void cmd_shutdown() {
        gs.shutdown();
    }

private:
    game_state& gs;
};

} // namespace dfdh
