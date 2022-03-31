#pragma once

#include <array>
#include <stdexcept>
#include <string>
#include <memory>
#include <set>
#include <filesystem>

#include "split_view.hpp"
#include "log.hpp"
#include "io_tools.hpp"
#include "ston.hpp"

namespace dtls
{
template <char... Chars, size_t... Idxs>
constexpr bool str_starts_with(const char* str, std::index_sequence<Idxs...>) {
    return ((str[Idxs] == Chars) && ...);
}
} // namespace dtls

template <char... Chars>
constexpr bool str_starts_with(const char* str) {
    return dtls::str_starts_with<Chars...>(str, std::make_index_sequence<sizeof...(Chars)>());
}

template <char... Chars>
constexpr const char* str_find(const char* str) {
    return (*str == '\0' || str_starts_with<Chars...>(str)) ? str : str_find<Chars...>(str + 1);
}

template <typename T>
constexpr auto typename_str() {
    constexpr auto pretty_func = __PRETTY_FUNCTION__;
    constexpr auto start_size  = sizeof(__PRETTY_FUNCTION__);
    constexpr auto type_start  = str_find<'T', ' ', '=', ' '>(pretty_func);
    constexpr auto skipped     = type_start - pretty_func;
    return std::string_view(type_start + 4, start_size - skipped - 6);
}

template <typename T, T z>
constexpr auto enum_name() {
    constexpr auto pretty_func = __PRETTY_FUNCTION__;
    constexpr auto start_size  = sizeof(__PRETTY_FUNCTION__);
    constexpr auto type_start  = str_find<'z', ' ', '=', ' '>(pretty_func);
    constexpr auto skipped     = type_start - pretty_func;
    return std::string_view(type_start + 4, start_size - skipped - 6);
}

template <typename T>
auto enum_name(T enum_value) {
#define enum_block(N)                                                                              \
    case N:                                                                                        \
        return enum_name<T, static_cast<T>(N)>()
#define enum_block2(N)                                                                             \
    enum_block(N);                                                                                 \
    enum_block(N + 1)
#define enum_block4(N)                                                                             \
    enum_block2(N);                                                                                \
    enum_block2(N + 2)
#define enum_block8(N)                                                                             \
    enum_block4(N);                                                                                \
    enum_block4(N + 4)
#define enum_block16(N)                                                                            \
    enum_block8(N);                                                                                \
    enum_block8(N + 8)

    switch (int(enum_value)) {
        enum_block16(-1);
    default: return std::string_view();
    }

#undef enum_block
#undef enum_block2
#undef enum_block4
#undef enum_block8
#undef enum_block16
}

namespace dfdh
{

class cfg_not_found : public std::runtime_error {
public:
    cfg_not_found(const std::string& fname):
        std::runtime_error("Config file " + fname + " was not found") {}
};

class cfg_duplicate_section : public std::runtime_error {
public:
    cfg_duplicate_section(const std::string& section):
        std::runtime_error("Duplicate section [" + section + "]") {}
};

class cfg_invalid_section : public std::runtime_error {
public:
    cfg_invalid_section(const std::string& section):
        std::runtime_error("Invalid section declaration: " + section) {}
};

class cfg_file_commit_error : public std::runtime_error {
public:
    cfg_file_commit_error(const std::string& filepath, const std::string& msg):
        std::runtime_error("Failed to commit file \"" + filepath + "\": " + msg) {}
};

enum class cfg_token_t {
    sect_declaration = 0,
    key,
    value,
    whitespace,
    eq,
    comment,
    include_macro,
    HEAD_NODE
};

struct cfg_token {
    cfg_token_t type;
    std::string value;
};

template <typename I, typename EndI>
class cfg_tokenizer {
public:
    cfg_tokenizer(I ibegin, EndI iend): beg(ibegin), end(iend) {}

    enum class state_t { on_section_name = 0, on_key, on_eq, on_value, on_macro };

    operator bool() const {
        return beg != end || cur_pos != cur_end;
    }

    template <char... Chars>
    static constexpr bool any_of(char c) {
        return ((c == Chars) || ...);
    }

    bool quote_op(char c, bool& v, char quote) {
        if (v) {
            tk.push_back(c);
            if (c == quote)
                v = false;
            return true;
        }
        else {
            if (c == quote) {
                tk.push_back(c);
                v = true;
                return true;
            }
        }
        return false;
    }

    enum analyze_st { a_next = 1, a_changed = 1 << 1 };

    int analyze(char c) {
        if (quote_op(c, on_quotes, '\'') || quote_op(c, on_double_quotes, '"')) {
            whitespace_frontier = false;
            last_char           = c;
            return a_next;
        }

        if (whitespace_frontier) {
            switch (c) {
            case '[':
                state               = state_t::on_section_name;
                whitespace_frontier = false;
                return a_changed;
            case '#':
                state               = state_t::on_macro;
                whitespace_frontier = false;
                return a_changed;
            case '\n': whitespace_frontier = true;
            case ' ':
            case '\t':
            case '\r': break;
            default:
                if (state != state_t::on_key && last_char == '\n') {
                    state = state_t::on_key;
                    return a_changed;
                }
                whitespace_frontier = false;
            }
        }
        else {
            if (c == '\n') {
                whitespace_frontier = true;
                if (state == state_t::on_macro) {
                    state = state_t::on_key;
                    return a_changed;
                }
            }
        }

        switch (state) {
        case state_t::on_section_name:
            tk.push_back(c);
            if (c == ']') {
                state     = state_t::on_key;
                last_char = c;
                return a_next | a_changed;
            }
            break;
        case state_t::on_key:
            if (c == '=') {
                state = state_t::on_eq;
                return a_changed;
            }
            else
                tk.push_back(c);
            break;
        case state_t::on_eq:
            tk.push_back(c);
            state     = state_t::on_value;
            last_char = c;
            return a_next | a_changed;
        case state_t::on_value:
        case state_t::on_macro: tk.push_back(c); break;
        }

        last_char = c;
        return a_next;
    }

    struct tk_t {
        state_t     state;
        std::string value;
    };

    tk_t next_tk() {
        auto prev_state = state;
        while (beg != end) {
            auto st = analyze(*beg);
            if (st & a_next)
                ++beg;
            if (st & a_changed)
                break;
        }

        auto res = std::move(tk);
        return {prev_state, res};
    }

    cfg_token_t state_to_token_t(state_t state, const std::string& value) {
        switch (state) {
        case state_t::on_section_name: return cfg_token_t::sect_declaration;
        case state_t::on_key: return cfg_token_t::key;
        case state_t::on_eq: return cfg_token_t::eq;
        case state_t::on_value: return cfg_token_t::value;
        case state_t::on_macro:
            size_t pos = 1;
            while (any_of<' ', '\t'>(value[pos])) ++pos;
            if (std::string_view(value).substr(pos).starts_with("include") &&
                any_of<' ', '\t'>(value[pos + sizeof("include") - 1]))
                return cfg_token_t::include_macro;
            return cfg_token_t::comment;
        }
        return cfg_token_t::value;
    }

    cfg_token next() {
        if (cur_pos == cur_end) {
            while (true) {
                auto tk = next_tk();
                if (tk.value.empty())
                    continue;
                size_t pos = 0;
                while (any_of<' ', '\t', '\r', '\n'>(tk.value[pos])) ++pos;
                if (pos != 0)
                    put(cfg_token_t::whitespace, tk.value.substr(0, pos));

                if (pos == tk.value.size())
                    break;

                size_t pos2 = tk.value.size();
                while (any_of<' ', '\t', '\r', '\n'>(tk.value[pos2 - 1]) && pos2 - 1 != 0) --pos2;

                if (pos != pos2) {
                    auto val = tk.value.substr(pos, pos2 - pos);
                    put(state_to_token_t(tk.state, val), val);
                }
                if (pos2 != tk.value.size())
                    put(cfg_token_t::whitespace, tk.value.substr(pos2));
                break;
            }
        }
        auto pos = cur_pos;
        cur_pos  = (cur_pos + 1) & 0x3;
        return cur_tokens[pos];
    }

    [[nodiscard]]
    cfg_token current() const {
        return cur_tokens[(cur_pos - 1) & 0x3];
    }

private:
    void put(cfg_token_t type, std::string value) {
        cur_tokens[cur_end].type  = type;
        cur_tokens[cur_end].value = std::move(value);
        cur_end                   = (cur_end + 1) & 0x3;
    }

private:
    I           beg;
    EndI        end;
    std::string tk;
    state_t     state               = state_t::on_key;
    bool        on_quotes           = false;
    bool        on_double_quotes    = false;
    bool        whitespace_frontier = true;
    char        last_char           = '\0';

    std::array<cfg_token, 4> cur_tokens;
    size_t                   cur_pos = 0;
    size_t                   cur_end = 0;
};

template <bool Const, bool EndIterator>
class cfg_file_iterator;

struct cfg_file_node {
    cfg_file_node(std::string ipath, struct cfg_node* istart):
        path(std::move(ipath)), start(istart) {}

    cfg_file_iterator<false, false>              begin();
    cfg_file_iterator<false, true>               end();
    [[nodiscard]] cfg_file_iterator<true, false> begin() const;
    [[nodiscard]] cfg_file_iterator<true, true>  end() const;

    void commit(bool force = false);

    friend std::ostream& operator<<(std::ostream& os, const cfg_file_node& file_node);

    std::string              path;
    struct cfg_node*         start;
    std::set<cfg_file_node*> inner;
    bool                     commit_required = false;
};

struct cfg_node {
    static std::unique_ptr<cfg_node> create(cfg_token_t type, std::string value) {
        return std::make_unique<cfg_node>(type, std::move(value));
    }

    cfg_node(cfg_token_t type, std::string value): tk{type, std::move(value)} {}

    cfg_node* insert_after(std::unique_ptr<cfg_node> node, bool commit_required = true) {
        if (file && commit_required)
            file->commit_required = true;

        node->file     = file;
        auto next_next = std::move(next);
        next           = std::move(node);
        next->prev     = this;
        next->next     = std::move(next_next);
        if (next->next)
            next->next->prev = next.get();
        return next.get();
    }

    cfg_node* remove_after(bool commit_required = true) {
        if (next) {
            if (next->file && commit_required)
                next->file->commit_required = true;

            next = std::move(next->next);
            if (next)
                next->prev = this;
        }
        return next.get();
    }

    cfg_token                 tk;
    std::unique_ptr<cfg_node> next;
    cfg_node*                 prev = nullptr;
    cfg_file_node*            file = nullptr;
};

template <bool Const, typename Derived>
class cfg_iterator_base {
public:
    using node_t = std::conditional_t<Const, const cfg_node*, cfg_node*>;

    cfg_iterator_base(node_t inode = nullptr): node(inode) {
        while (node && (node->tk.type == cfg_token_t::include_macro ||
                        node->tk.type == cfg_token_t::HEAD_NODE))
            node = node->next.get();
    }

    auto& operator*() {
        return node->tk;
    }

    auto& operator*() const {
        return node->tk;
    }

    auto operator->() {
        return &node->tk;
    }

    auto operator->() const {
        return &node->tk;
    }

    Derived& operator++() {
        node = node->next.get();
        while (node && node->tk.type == cfg_token_t::include_macro)
            node = node->next.get();
        return static_cast<Derived&>(*this);
    }

    Derived& operator--() {
        node = node->prev;
        while (node && node->tk.type == cfg_token_t::include_macro)
            node = node->next.get();
        return static_cast<Derived&>(*this);
    }

    Derived& operator++(int) {
        auto res = *this;
        ++(*this);
        return static_cast<Derived&>(*this);
    }

    Derived& operator--(int) {
        auto res = *this;
        --(*this);
        return static_cast<Derived&>(*this);
    }

    node_t raw_node() const {
        return node;
    }

protected:
    node_t node;
};

template <bool Const>
class cfg_plain_iterator : public cfg_iterator_base<Const, cfg_plain_iterator<Const>> {
public:
    using cfg_iterator_base<Const, cfg_plain_iterator<Const>>::cfg_iterator_base;

    [[nodiscard]]
    bool operator==(const cfg_plain_iterator& rhs) const {
        return this->node == rhs.node;
    }

    [[nodiscard]]
    bool operator!=(const cfg_plain_iterator& rhs) const {
        return !(*this == rhs);
    }

    operator cfg_plain_iterator<true>() const {
        return {this->node};
    }

    [[nodiscard]]
    cfg_file_node* file_node() const {
        return this->node->file;
    }
};

template <bool Const, bool EndIterator>
class cfg_section_iterator : public cfg_iterator_base<Const, cfg_section_iterator<Const, EndIterator>> {
public:
    using cfg_iterator_base<Const, cfg_section_iterator<Const, EndIterator>>::cfg_iterator_base;

    [[nodiscard]]
    bool operator==(const cfg_section_iterator& rhs) const {
        return this->node == rhs.node;
    }

    [[nodiscard]]
    bool operator!=(const cfg_section_iterator& rhs) const {
        return !(*this == rhs);
    }

    operator cfg_section_iterator<true, EndIterator>() const {
        return {this->node};
    }

    friend bool operator==(const cfg_section_iterator& lhs, const cfg_section_iterator<Const, true>& rhs) {
        return !lhs.node || (lhs.node->tk.type == cfg_token_t::sect_declaration && lhs.node != rhs.raw_node());
    }
    friend bool operator==(const cfg_section_iterator<Const, true>& lhs, const cfg_section_iterator& rhs) {
        return lhs == rhs;
    }

    friend bool operator!=(const cfg_section_iterator& lhs, const cfg_section_iterator<Const, true>& rhs) {
        return !(lhs == rhs);
    }
    friend bool operator!=(const cfg_section_iterator<Const, true>& lhs, const cfg_section_iterator& rhs) {
        return rhs != lhs;
    }
};

template <bool Const>
class cfg_section_iterator<Const, true> {
public:
    cfg_section_iterator(const cfg_node* inode = nullptr): node(inode) {
        while (node && (node->tk.type == cfg_token_t::HEAD_NODE ||
                        node->tk.type == cfg_token_t::include_macro))
            node = node->next.get();
    }

    operator cfg_section_iterator<true, true>() const {
        return {node};
    }

    [[nodiscard]]
    const cfg_node* raw_node() const {
        return node;
    }

private:
    const cfg_node* node;
};

template <bool Const, bool EndIterator>
class cfg_file_iterator : public cfg_iterator_base<Const, cfg_file_iterator<Const, EndIterator>> {
public:
    using cfg_iterator_base<Const, cfg_file_iterator<Const, EndIterator>>::cfg_iterator_base;

    [[nodiscard]]
    bool operator==(const cfg_file_iterator& rhs) const {
        return this->node == rhs.node;
    }

    [[nodiscard]]
    bool operator!=(const cfg_file_iterator& rhs) const {
        return !(*this == rhs);
    }

    operator cfg_file_iterator<true, EndIterator>() const {
        return {this->node};
    }

    friend bool operator==(const cfg_file_iterator& lhs, const cfg_file_iterator<Const, true>& rhs) {
        return !lhs.node || !lhs.node->file->inner.contains(rhs.raw_node()->file);
    }
    friend bool operator==(const cfg_file_iterator<Const, true>& lhs, const cfg_file_iterator& rhs) {
        return lhs == rhs;
    }

    friend bool operator!=(const cfg_file_iterator& lhs, const cfg_file_iterator<Const, true>& rhs) {
        return !(lhs == rhs);
    }
    friend bool operator!=(const cfg_file_iterator<Const, true>& lhs, const cfg_file_iterator& rhs) {
        return rhs != lhs;
    }
};

template <bool Const>
class cfg_file_iterator<Const, true> {
public:
    cfg_file_iterator(const cfg_node* inode = nullptr): node(inode) {
        if (node && node->tk.type == cfg_token_t::HEAD_NODE)
            node = node->next.get();
    }

    operator cfg_file_iterator<true, true>() const {
        return {node};
    }

    [[nodiscard]]
    const cfg_node* raw_node() const {
        return node;
    }

private:
    const cfg_node* node;
};

inline cfg_file_iterator<false, false> cfg_file_node::begin() {
    return {start};
}

inline cfg_file_iterator<false, true>  cfg_file_node::end() {
    return {start};
}

inline cfg_file_iterator<true, false>  cfg_file_node::begin() const {
    return {start};
}

inline cfg_file_iterator<true, true>   cfg_file_node::end() const {
    return {start};
}

inline void cfg_file_node::commit(bool force) {
    if (!commit_required && !force)
        return;

    auto ofs = std::ofstream(path, std::ios::binary | std::ios::out);

    if (!ofs.is_open())
        throw cfg_file_commit_error(path, "Can't open file for writing");

    for (auto i = start; i && i->file->inner.contains(this); i = i->next.get()) {
        if (i->file != this)
            continue;
        ofs.write(i->tk.value.data(), std::streamsize(i->tk.value.size()));
    }

    commit_required = false;
}

inline std::ostream& operator<<(std::ostream& os, const cfg_file_node& file_node) {
    for (auto& tk : file_node)
        os << tk.value;
    return os;
}

template <typename T>
concept CfgPushBackable = requires (T& v) {
    v.push_back(*v.begin());
};

template <typename T>
concept CfgOptional = requires (T& v) {
    {T()};
    {v} -> std::convertible_to<T>;
    {v.value()} -> std::convertible_to<decltype(*v)>;
};

template <typename T>
concept CfgTupleLike = requires (T& v) {
    {std::tuple_size<T>::value} -> std::convertible_to<size_t>;
};

template <typename U>
std::decay_t<U> cfg_cast(const std::string& str);

template <typename T, size_t I = 0>
void cfg_tuple_cast(auto b, auto e, T& res) {
    using std::get;
    if (b == e) {
        LOG_WARN(
            "Config cast error: {} arguments required ({} provided)", std::tuple_size_v<T>, I);
        return;
    }

    get<I>(res) = cfg_cast<std::tuple_element_t<I, T>>(std::string((*b).begin(), (*e).end()));
    auto next = std::next(b);
    if constexpr (I + 1 == std::tuple_size_v<T>) {
        if (next != e)
            LOG_WARN("Config cast error: arguments out of space");
        return;
    }
    else {
        cfg_tuple_cast<T, I + 1>(next, e, res);
    }
}

template <typename U>
std::decay_t<U> cfg_cast(const std::string& str) {
    using T = std::decay_t<U>;

    if constexpr (std::is_same_v<T, bool>) {
        if (str == "true" || str == "on")
            return true;
        else if (str == "false" || str == "off")
            return false;

        LOG_WARN("Config cast error: {} not a bool", str);
        return false;
    }
    else if constexpr (Number<T>) {
        T res = T(0);
        try {
            res = ston<T>(str);
        }
        catch (const std::exception& e) {
            LOG_WARN("Config cast error: {} not a number", str);
        }
        return res;
    }
    else if constexpr (std::is_same_v<T, std::string>) {
        return str;
    }
    else if constexpr (CfgPushBackable<T>) {
        T res;
        for (auto v : str / split_when(skip_whitespace_outside_quotes{}))
            res.push_back(cfg_cast<decltype(*res.begin())>(std::string(v.begin(), v.end())));
        return res;
    }
    else if constexpr (CfgOptional<T>) {
        if (str == "null")
            return T();
        else
            return cfg_cast<decltype(*T())>(str);
    }
    else if constexpr (CfgTupleLike<T>) {
        T res{};
        auto splits = str / split_when(skip_whitespace_outside_quotes{});
        cfg_tuple_cast(splits.begin(), splits.end(), res);
        return res;
    }
    else {
        enum class fail {};
        return fail{};
    }
}

template <typename T>
concept CfgBeginEndable = requires (const T& v) {
    v.begin(); v.end();
};

template <typename T, size_t Nesting = 0>
std::string cfg_str_cast(const T& val);

template <typename T, size_t Nesting, size_t... Idxs>
std::string cfg_tuple_str_cast(const T& val, std::index_sequence<Idxs...>) {
    using std::get;

    std::string res;

    if constexpr (Nesting == 1)
        res.push_back('\'');
    if constexpr (Nesting == 2)
        res.push_back('"');

    ((res += cfg_str_cast<decltype(get<Idxs>(val)), Nesting + 1>(get<Idxs>(val)) + " "), ...);

    res.pop_back();

    if constexpr (Nesting == 1)
        res.push_back('\'');
    if constexpr (Nesting == 2)
        res.push_back('"');

    return res;
}

template <typename U, size_t Nesting>
std::string cfg_str_cast(const U& val) {
    using T = std::decay_t<U>;
    static_assert(Nesting < 3, "Nesting must be < 3");

    if constexpr (std::is_same_v<T, bool>) {
        return val ? "true" : "false";
    }
    else if constexpr (Number<T>) {
        /* XXX: stringstream is very slow */
        std::stringstream ss;
        ss << std::setprecision(9) << val;
        return ss.str();
    }
    else if constexpr (std::is_same_v<T, std::string> || std::is_convertible_v<T, const char*>) {
        return val;
    }
    else if constexpr (CfgBeginEndable<T>) {
        if (val.begin() == val.end())
            return {};

        std::string res;
        if constexpr (Nesting == 1)
            res.push_back('\'');
        if constexpr (Nesting == 2)
            res.push_back('"');

        for (auto& v : val) {
            res.push_back(cfg_str_cast<decltype(v), Nesting + 1>(v));
            res.push_back(' ');
        }
        res.pop_back();

        if constexpr (Nesting == 1)
            res.push_back('\'');
        if constexpr (Nesting == 2)
            res.push_back('"');

        return res;
    }
    else if constexpr (CfgOptional<T>) {
        if (val)
            return cfg_str_cast<decltype(*val), Nesting>(*val);
        return "null";
    }
    else if constexpr (CfgTupleLike<T>) {
        return cfg_tuple_str_cast<T, Nesting>(val, std::make_index_sequence<std::tuple_size_v<T>>());
    }
    else {
        enum class fail {};
        return fail{};
    }
}

class cfg_no_value_exception : public std::runtime_error {
public:
    cfg_no_value_exception(const std::string& section, const std::string& key):
        std::runtime_error("Key '" + key + "' in section [" + section +
                           "] does not contain a value") {}
};

template <typename T, bool Const>
class cfg_value {
public:
    cfg_value(cfg_node* isection, cfg_node* ikey, cfg_node* ivalue):
        section(isection), key(ikey), node(ivalue) {}

    [[nodiscard]]
    std::string raw_value() const {
        return has_value() ? node->tk.value : std::string();
    }

    [[nodiscard]]
    T value() const {
        if (has_value())
            return cfg_cast<T>(node->tk.value);
        else
            throw cfg_no_value_exception(
                section->tk.type == cfg_token_t::sect_declaration ? section->tk.value : std::string(),
                key->tk.value);
    }

    [[nodiscard]]
    std::optional<T> try_get() const {
        return has_value() ? cfg_cast<T>(node->tk.value) : std::optional<T>();
    }

    operator bool() const {
        return has_value();
    }

    [[nodiscard]]
    bool has_value() const {
        return node->tk.type == cfg_token_t::value;
    }

    cfg_value& operator=(const T& value) const {
        set(value);
        return *this;
    }

    [[nodiscard]]
    bool operator==(const cfg_value& rhs) const {
        return raw_value() == rhs.raw_value();
    }

    [[nodiscard]]
    bool operator!=(const cfg_value& rhs) const {
        return !(*this == rhs);
    }

protected:
    /* May be cfg_token_t::eq or cfg_token_t::value only */
    cfg_node* section;
    cfg_node* key;
    cfg_node* node;
};

template <typename T>
class cfg_value<T, false> : public cfg_value<T, true> {
public:
    using cfg_value<T, true>::cfg_value;

    void raw_set(std::string value) {
        if (value.empty()) {
            clear();
            return;
        }

        if (this->has_value()) {
            this->node->tk.value = std::move(value);
            if (this->node->file)
                this->node->file->commit_required = true;
        }
        else {
            auto prev = this->node->prev;
            /* Insert new '=' and whitespace before old '=' */
            prev->insert_after(cfg_node::create(cfg_token_t::eq, "="))
                ->insert_after(cfg_node::create(cfg_token_t::whitespace, " "));

            /* The old '=' becomes value */
            this->node->tk.type  = cfg_token_t::value;
            this->node->tk.value = std::move(value);
        }
    }

    void set(const T& value) {
        raw_set(cfg_str_cast(value));
    }

    void clear() {
        if (this->has_value()) {
            /* Find '=' sign */
            auto eq = this->node;
            while (eq->tk.type != cfg_token_t::eq)
                eq = eq->prev;

            /* Take one before '=' */
            auto prev = eq->prev;

            /* Remove '=' and all whitespaces before the value */
            prev->remove_after();
            while (prev->next->tk.type == cfg_token_t::whitespace)
                prev->remove_after();

            /* Value becomes '=' sign */
            prev->next->tk.type  = cfg_token_t::eq;
            prev->next->tk.value = "=";
        }
    }
};

struct cfg_key {
    [[nodiscard]]
    bool operator<(const cfg_key*& node) const {
        return key->tk.value < node->key->tk.value;
    }
    cfg_node* key;
};

struct cfg_key_cmp {
    using is_transparent = void;

    bool operator()(const cfg_key& lhs, const std::string& rhs) const {
        return lhs.key->tk.value < rhs;
    }

    bool operator()(const cfg_key& lhs, const cfg_key& rhs) const {
        return operator()(lhs, rhs.key->tk.value);
    }

    bool operator()(const std::string& lhs, const cfg_key& rhs) const {
        return lhs < rhs.key->tk.value;
    }
};

class cfg_key_not_found : public std::runtime_error {
public:
    cfg_key_not_found(const std::string& section, const std::string& key):
        std::runtime_error("Key '" + key + "' not found in section [" + section + "]") {}
};

template <bool Const>
class cfg_section {
public:
    cfg_section(cfg_node* isection_start): start(isection_start) {}

    [[nodiscard]]
    auto& get_values() const {
        return values;
    }

    template <typename T>
    cfg_value<T, true> get(const std::string& key) const {
        auto found = values.find(key);
        if (found != values.end())
            return cfg_value<T, true>(start, found->first.key, found->second);
        else
            throw cfg_key_not_found(section_name(), key);
    }

    template <typename T>
    std::optional<cfg_value<T, true>> try_get(const std::string& key) const {
        auto found = values.find(key);
        if (found != values.end())
            return cfg_value<T, true>(start, found->first.key, found->second);
        else
            return {};
    }

    [[nodiscard]]
    bool has_key(const std::string& key) const {
        return values.contains(key);
    }

    template <typename T>
    T value_or_default(const std::string& key, T default_value) const {
        if (auto v = try_get<T>(key))
            return *v ? v->value() : default_value;
        else
            return default_value;
    }

    [[nodiscard]]
    std::set<std::string> list_keys() const {
        std::set<std::string> result;
        for (auto& [key, _] : values)
            result.insert(key.key->tk.value);
        return result;
    }

    [[nodiscard]]
    const std::string& section_name() const {
        return start->tk.value;
    }

    [[nodiscard]]
    cfg_section_iterator<true, false> begin() const {
        return {start};
    }

    [[nodiscard]]
    cfg_section_iterator<true, true> end() const {
        return {start};
    }

    friend std::ostream& operator<<(std::ostream& os, const cfg_section& sect) {
        for (auto& tk : sect)
            os << tk.value;
        return os;
    }

    [[nodiscard]]
    bool operator==(const cfg_section& rhs) const {
        if (section_name() != rhs.section_name())
            return false;

        for (auto& [k, v] : values) {
            auto found = rhs.values.find(k);
            if (found == rhs.values.end())
                return false;
            if (v->tk.value != found->second->tk.value || v->tk.type != found->second->tk.type)
                return false;
        }
        return true;
    }

    [[nodiscard]]
    bool operator!=(const cfg_section& rhs) const {
        return !(*this == rhs);
    }

protected:
    friend class cfg;

    std::map<cfg_key, cfg_node*, cfg_key_cmp> values;
    cfg_node*                                 start;
};

inline size_t cfg_section_key_check(const std::string& key) {
    enum state_t { ok = 0, single_quote, double_quote } state = ok;
    for (size_t pos = 0; pos < key.size(); ++pos) {
        char c = key[pos];
        switch (state) {
        case ok:
            switch (c) {
            case '=':
                return pos;
            case '\'':
                state = single_quote;
                break;
            case '"':
                state = double_quote;
                break;
            }
            break;
        case single_quote:
            if (c == '\'')
                state = ok;
            break;
        case double_quote:
            if (c == '"')
                state = ok;
            break;
        }
    }
    return std::string::npos;
}

enum class cfg_settings_insert {
    after_section_declaration = 0, // Fastest
    after_last_key,
    lexicographical_compare_key
};

enum class cfg_settings_indentation {
    off = 0,
    spaces
};

class cfg_settings_singleton {
public:
    static cfg_settings_singleton& instance() {
        static cfg_settings_singleton inst;
        return inst;
    }

    cfg_settings_singleton(const cfg_settings_singleton&) = delete;
    cfg_settings_singleton& operator=(const cfg_settings_singleton&) = delete;

private:
    cfg_settings_singleton() = default;
    ~cfg_settings_singleton() = default;

public:
    cfg_settings_insert      insert = cfg_settings_insert::after_section_declaration;
    cfg_settings_indentation indent = cfg_settings_indentation::off;
};

inline cfg_settings_singleton& cfg_settings() {
    return cfg_settings_singleton::instance();
}

template <>
class cfg_section<false> : public cfg_section<true> {
public:
    using cfg_section<true>::cfg_section;

    template <typename T>
    cfg_value<T, false> get(const std::string& key) {
        auto found = values.find(key);
        if (found != values.end())
            return {start, found->first.key, found->second};
        else
            throw cfg_key_not_found(section_name(), key);
    }

    template <typename T>
    std::optional<cfg_value<T, false>> try_get(const std::string& key) {
        auto found = values.find(key);
        if (found != values.end())
            return cfg_value<T, false>{start, found->first.key, found->second};
        else
            return {};
    }

    template <typename T>
    cfg_value<T, false> access(const std::string& key) {
        auto found = this->values.find(key);
        if (found != this->values.end())
            return {this->start, found->first.key, found->second};

        auto [key_node, value_node] = try_insert(key);
        return {this->start, key_node, value_node};
    }

    void raw_set(const std::string& key, const std::string& value) {
        access<std::string>(key).raw_set(value);
    }

    template <typename T>
    cfg_value<T, false> set(const std::string& key, const T& value) {
        auto v = access<T>(key);
        v.set(value);
        return v;
    }

    template <typename T>
    T value_or_default_and_set(const std::string& key, const T& default_value) {
        if (auto v = try_get<T>(key)) {
            if (*v)
                return v->value();
            v->set(default_value);
            return default_value;
        }
        else {
            set(key, default_value).value();
        }
    }

    bool delete_key(const std::string& key) {
        auto found = values.find(key);
        if (found == values.end())
            return false;

        auto before_key  = found->first.key->prev;
        if (before_key->tk.type == cfg_token_t::whitespace && before_key->tk.value.back() == '\n')
            before_key = before_key->prev;
        auto after_value = found->second->next.get();

        while (before_key->remove_after() != after_value);

        values.erase(found);

        return true;
    }

    void insert_breakline() {
        if (cfg_settings().insert != cfg_settings_insert::after_last_key)
            LOG_WARN(DFDH_SOURCELINE"cfg_section: inserting breakline does not affect following key inserts, "
                     "because insert mode is not cfg_settings_insert::after_last_key");

        auto last = find_last_insert_pos();
        if (last->tk.type == cfg_token_t::whitespace)
            last->tk.value.push_back('\n');
        else {
            if (last->next->tk.type == cfg_token_t::whitespace)
                last->next->tk.value.push_back('\n');
            else
                last->insert_after(cfg_node::create(cfg_token_t::whitespace, "\n"));
        }
    }

    void clear() {
        while (true) {
            auto next = start->remove_after();
            /* Break if next token (>) is:
             *
             * >[next_section] or >EOF
             * 
             * or:
             *
             * >                 >
             * [next_section] or EOF
             */
            if (!next || next->tk.type == cfg_token_t::sect_declaration ||
                (next->tk.type == cfg_token_t::whitespace &&
                 (!next->next || next->next->tk.type == cfg_token_t::sect_declaration)))
                break;
        }
        values.clear();
    }

    cfg_section_iterator<false, false> begin() {
        return {start};
    }

    cfg_section_iterator<false, true> end() {
        return {start};
    }

    void unsafe_remove_entire_data() {
        clear();
        start->prev->remove_after();
        start = nullptr;
        values.clear();
    }

private:
    std::pair<cfg_node*, cfg_node*> try_insert(std::string key) {
        size_t found_eq = cfg_section_key_check(key);
        if (found_eq != std::string::npos) {
            LOG_WARN("cfg_section: attempt to setup key with '=' sign: '{}'", key);
            key = key.substr(0, found_eq);
            LOG_WARN("cfg_section:                                       ^-this key will be truncated: '{}'", key);
            auto found_pair = values.find(key);
            if (found_pair != values.end())
                return {found_pair->first.key, found_pair->second};
        }

        cfg_node* key_node = this->start;
        cfg_node* value_node;

        switch (cfg_settings().insert) {
        case cfg_settings_insert::lexicographical_compare_key:
            key_node = find_lexicographicaly_prev(key);
            break;
        case cfg_settings_insert::after_last_key:
            key_node = find_last_insert_pos();
            break;
        default: break;
        }

        key_node = key_node->insert_after(cfg_node::create(cfg_token_t::whitespace, "\n"))
                       ->insert_after(cfg_node::create(cfg_token_t::key, key));

        size_t space = 0;
        switch (cfg_settings().indent) {
        case cfg_settings_indentation::spaces:
            if (cfg_settings().insert == cfg_settings_insert::after_section_declaration)
                space = count_key_space_lookahead(key_node);
            else {
                space = count_key_space_lookbefore(key_node);
                if (space == 0)
                    space = count_key_space_lookahead(key_node);
            }

            value_node = key_node->insert_after(cfg_node::create(
                cfg_token_t::whitespace,
                space > key.size() + 1 ? std::string(space - key.size(), ' ') : " "));
            break;

        case cfg_settings_indentation::off:
        default:
            value_node = key_node->insert_after(cfg_node::create(cfg_token_t::whitespace, " "));
            break;
        }

        value_node = value_node->insert_after(cfg_node::create(cfg_token_t::eq, "="));
        values.insert_or_assign(cfg_key{key_node}, value_node);
        return {key_node, value_node};
    }

    [[nodiscard]]
    cfg_node* find_last_insert_pos() const {
        auto last = this->start;
        for (auto next = last->next.get(); next && next->tk.type != cfg_token_t::sect_declaration;
             next      = next->next.get())
            if (next->tk.type == cfg_token_t::value)
                last = next;

        static constexpr auto is_multiple_breakline = [](cfg_node* node) {
            /* std::count may be faster than checking count in the loop and break after > 1
             * (bench this?)
             */
            return node && node->tk.type == cfg_token_t::whitespace &&
                   std::count(node->tk.value.begin(), node->tk.value.end(), '\n') > 1;
        };

        if (last->tk.type == cfg_token_t::value && is_multiple_breakline(last->next.get()))
            last = last->next.get();

        return last;
    }

    /* XXX: use find in values std::map */
    [[nodiscard]]
    cfg_node* find_lexicographicaly_prev(const std::string& key) const {
        auto last = this->start;
        for (auto next = last->next.get(); next && next->tk.type != cfg_token_t::sect_declaration;
             next      = next->next.get()) {
            switch (next->tk.type) {
            case cfg_token_t::value: last = next; break;
            case cfg_token_t::key:
                if (next->tk.value > key)
                    return last;
                break;
            default: break;
            }
        }
        return last;
    }

    [[nodiscard]]
    size_t count_key_space_lookahead(cfg_node* key) const {
        if (key->tk.type != cfg_token_t::key)
            return 0;

        cfg_node* next_key = nullptr;
        for (auto next = key->next.get(); next && next->tk.type != cfg_token_t::sect_declaration;
             next      = next->next.get()) {
            if (next->tk.type == cfg_token_t::key) {
                next_key = next;
                break;
            }
        }

        return next_key ? count_key_space(next_key) : 0;
    }

    [[nodiscard]]
    size_t count_key_space_lookbefore(cfg_node* key) const {
        if (key->tk.type != cfg_token_t::key)
            return 0;

        cfg_node* prev_key = nullptr;
        for (auto prev = key->prev; prev && prev->tk.type != cfg_token_t::sect_declaration;
             prev      = prev->prev) {
            if (prev->tk.type == cfg_token_t::key) {
                prev_key = prev;
                break;
            }
        }

        return prev_key ? count_key_space(prev_key) : 0;
    }

    [[nodiscard]]
    size_t count_key_space(cfg_node* key) const {
        size_t res = key->tk.value.size();
        auto spaces = key->next.get();

        if (spaces->tk.type != cfg_token_t::whitespace)
            return res;

        for (auto c : spaces->tk.value) {
            switch (c) {
            case ' ':
                ++res;
                break;
            case '\t':
                res += 4; // TODO: customization
                break;
            default:
                res = 0;
            }
        }

        return res;
    }
};

namespace fs = std::filesystem;

class cfg_already_parsed : public std::runtime_error {
public:
    cfg_already_parsed(const std::string& path): std::runtime_error("File '" + path + "' already parsed") {}
};

class cfg_key_without_eq : public std::runtime_error {
public:
    cfg_key_without_eq(const std::string& section, const std::string& key):
        std::runtime_error("Invalid key '" + key + "' in section [" + section + "]") {}
};

class cfg_key_already_exists : public std::runtime_error {
public:
    cfg_key_already_exists(const std::string& section, const std::string& key):
        std::runtime_error("Key '" + key + "' already exists in section [" + section + "]") {}
};

class cfg_section_already_exists : public std::runtime_error {
public:
    cfg_section_already_exists(const std::string& section):
        std::runtime_error("Section [" + section + "] already exists") {}
};

class cfg_section_not_found : public std::runtime_error {
public:
    cfg_section_not_found(const std::string& section_name):
        std::runtime_error("Section [" + section_name + "] not found") {}
};

class cfg_file_not_parsed : public std::runtime_error {
public:
    cfg_file_not_parsed(const std::string& file_path):
        std::runtime_error("File '" + file_path + "' not parsed") {}
};

class cfg_cannot_insert_section : public std::runtime_error {
public:
    cfg_cannot_insert_section(const std::string& section_name):
        std::runtime_error("Cannot insert section [" + section_name + "]") {}
};

inline fs::path parse_include(const std::string& include) {
    auto pos = include.find("include") + sizeof("include") - 1;
    while (include[pos] == ' ' || include[pos] == '\t')
        ++pos;
    auto path = include.substr(pos);
    while (path.back() == ' ' || path.back() == '\t')
        path.pop_back();

    if (path.front() == '"' && path.back() == '"')
        return path.substr(1, path.size() - 2);
    return path;
}

struct cfg_section_name {
    cfg_section_name(std::string section_name): name(std::move(section_name)) {}
    std::string name;
};

struct cfg_file_path {
    cfg_file_path() = default;
    cfg_file_path(std::string file_name): path(fs::weakly_canonical(std::move(file_name)).string()) {}
    std::string path;
};

cfg_section_name operator "" _sect(const char* str, size_t size) {
    return {{str, size}};
}

cfg_file_path operator "" _file(const char* str, size_t size) {
    return {{str, size}};
}

class cfg {
public:
    cfg(const fs::path& entry_config_path) {
        head = cfg_node::create(cfg_token_t::HEAD_NODE, {});
        cfg_node* tail = head.get();
        parse(entry_config_path, tail);
    }

    void parse(const fs::path& config_path) {
        auto tail = calc_tail();
        parse(config_path, tail);
    }

    cfg_plain_iterator<false> begin() {
        return {head.get()};
    }

    [[nodiscard]]
    cfg_plain_iterator<true> begin() const {
        return {head.get()};
    }

    cfg_plain_iterator<false> end() {
        return {nullptr};
    }

    [[nodiscard]]
    cfg_plain_iterator<true> end() const {
        return {nullptr};
    }

    cfg_section<false>& get_section(const cfg_section_name& section_name) {
        auto found = sections.find(section_name.name);
        if (found == sections.end())
            throw cfg_section_not_found(section_name.name);
        return found->second;
    }

    [[nodiscard]]
    const cfg_section<true>& get_section(const cfg_section_name& section_name) const {
        auto found = sections.find(section_name.name);
        if (found == sections.end())
            throw cfg_section_not_found(section_name.name);
        return found->second;
    }

    cfg_section<false>* try_get_section(const cfg_section_name& section_name) {
        auto found = sections.find(section_name.name);
        return found != sections.end() ? &found->second : nullptr;
    }

    [[nodiscard]]
    const cfg_section<true>* try_get_section(const cfg_section_name& section_name) const {
        auto found = sections.find(section_name.name);
        return found != sections.end() ? &found->second : nullptr;
    }

    cfg_file_node& get_file(const cfg_file_path& file_path) {
        auto found = file_nodes.find(file_path.path);
        if (found == file_nodes.end())
            throw cfg_file_not_parsed(file_path.path);
        return *found->second;
    }

    [[nodiscard]]
    const cfg_file_node& get_file(const cfg_file_path& file_path) const {
        auto found = file_nodes.find(file_path.path);
        if (found == file_nodes.end())
            throw cfg_file_not_parsed(file_path.path);
        return *found->second;
    }

    cfg_file_node* try_get_file(const cfg_file_path& file_path) {
        auto found = file_nodes.find(file_path.path);
        return found != file_nodes.end() ? found->second.get() : nullptr;
    }

    [[nodiscard]]
    const cfg_file_node* try_get_file(const cfg_file_path& file_path) const {
        auto found = file_nodes.find(file_path.path);
        return found != file_nodes.end() ? found->second.get() : nullptr;
    }

    cfg_section<false>& operator[](const cfg_section_name& sect_name) {
        return sections.find(sect_name.name)->second;
    }

    [[nodiscard]]
    const cfg_section<false>& operator[](const cfg_section_name& sect_name) const {
        return sections.find(sect_name.name)->second;
    }

    cfg_file_node& operator[](const cfg_file_path& file_name) {
        return *file_nodes.find(file_name.path)->second;
    }

    [[nodiscard]]
    const cfg_file_node& operator[](const cfg_file_path& file_name) const {
        return *file_nodes.find(file_name.path)->second;
    }

    [[nodiscard]]
    const cfg_node* head_node() const {
        return head.get();
    }

    friend std::ostream& operator<<(std::ostream& os, const cfg& cfg) {
        for (auto& tk : cfg)
            os << tk.value;
        return os;
    }

    [[nodiscard]]
    std::set<std::string> list_sections() const {
        std::set<std::string> result;
        for (auto& s : sections)
            result.insert(s.first);
        return result;
    }

    void commit(bool force = false) {
        for (auto& [_, file] : file_nodes)
            file->commit(force);
    }

    bool try_remove_section(const cfg_section_name& section_name) {
        auto found = sections.find(section_name.name);
        if (found != sections.end()) {
            found->second.unsafe_remove_entire_data();
            sections.erase(found);
            return true;
        }
        return false;
    }

    void remove_section(const cfg_section_name& section_name) {
        if (!try_remove_section(section_name))
            throw cfg_section_not_found(section_name.name);
    }

    enum class insert_mode {
        at_the_end = 0,
        at_the_start,
        lexicographicaly
    };

    void skip_include(cfg_node*& p, cfg_file_node* file_node) {
        p       = p->next.get();
        auto p2 = p;
        for (; p && p->file != file_node; p2 = p, p = p->next.get());

        /* Insert node after include macro */
        if (!p) {
            p       = p2->insert_after(cfg_node::create(cfg_token_t::whitespace, "\n\n"));
            p->file = file_node;
        }
    }

    cfg_node* find_section_insert_pos(auto pos, cfg_file_node* file_node, insert_mode ins_mode) {
        switch (ins_mode) {
        case insert_mode::at_the_end: {
            if (file_node) {
                auto prev = file_node->start;
                if (prev->tk.type == cfg_token_t::include_macro)
                    prev = prev->prev;
                for (auto p = prev->next.get(); p && p->file == file_node; prev = p, p = p->next.get()) {
                    if (p->tk.type == cfg_token_t::include_macro)
                        skip_include(p, file_node);
                }
                return prev;
            }
            else {
                auto prev = head.get();
                for (auto p = head->next.get(); p; prev = p, p = p->next.get());
                return prev;
            }
        } break;
        case insert_mode::at_the_start: {
            if (file_node) {
                auto prev = file_node->start;
                if (prev->tk.type == cfg_token_t::include_macro)
                    prev = prev->prev;
                for (auto p = prev->next.get();
                     p && p->file == file_node && p->tk.type != cfg_token_t::sect_declaration;
                     prev = p, p = p->next.get())
                    if (p->tk.type == cfg_token_t::include_macro)
                        skip_include(p, file_node);
                return prev;
            }
            else {
                auto prev = head.get();
                for (auto p = head->next.get(); p && p->tk.type != cfg_token_t::sect_declaration;
                     prev = p, p = p->next.get());
                return prev;
            }
        } break;
        case insert_mode::lexicographicaly: {
            if (file_node) {
                auto next_sect_pos = std::next(pos);
                for (; next_sect_pos != sections.end() && next_sect_pos->second.start->file != file_node;
                     ++next_sect_pos);
                if (next_sect_pos == sections.end())
                    return find_section_insert_pos(pos, file_node, insert_mode::at_the_end);
                return next_sect_pos->second.start->prev;
            }
            else {
                auto next_sect_pos = std::next(pos);
                if (next_sect_pos == sections.end())
                    return find_section_insert_pos(pos, file_node, insert_mode::at_the_end);
                return next_sect_pos->second.start->prev;
            }
        } break;
        }

        return nullptr;
    }

    /* Creates new section
     * if file_path is empty, the section was created somewhere in the whole config
     */
    cfg_section<false> create_section(const cfg_section_name& section_name,
                                      const cfg_file_path&    file_path     = {},
                                      insert_mode             insert_mode_v = insert_mode::lexicographicaly) {
        auto [pos, was_insert] = sections.emplace(section_name.name, cfg_section<false>{nullptr});
        if (!was_insert)
            throw cfg_section_already_exists(section_name.name);

        cfg_file_node* file_node = nullptr;
        if (!file_path.path.empty()) {
            auto found = file_nodes.find(file_path.path);
            if (found == file_nodes.end())
                throw cfg_not_found(file_path.path);
            file_node = found->second.get();
        }

        auto& new_section = pos->second;
        cfg_node* insert_pos = find_section_insert_pos(pos, file_node, insert_mode_v);
        if (!insert_pos || /* XXX: fix this */ insert_pos->tk.type == cfg_token_t::include_macro)
            throw cfg_cannot_insert_section(section_name.name);

        if (insert_pos->tk.type != cfg_token_t::whitespace) {
            insert_pos = insert_pos->insert_after(cfg_node::create(cfg_token_t::whitespace, "\n\n"));
            if (file_node)
                insert_pos->file = file_node;
        }
        else if (!insert_pos->tk.value.ends_with("\n\n")) {
            insert_pos->tk.value.push_back('\n');
            if (!insert_pos->tk.value.ends_with("\n\n"))
                insert_pos->tk.value.push_back('\n');
        }

        auto sect_node = new_section.start =
            insert_pos->insert_after(cfg_node::create(cfg_token_t::sect_declaration, "[" + section_name.name + "]"));
        if (file_node)
            sect_node->file = file_node;

        if (!sect_node->next || sect_node->next->tk.type != cfg_token_t::whitespace)
            sect_node->insert_after(cfg_node::create(cfg_token_t::whitespace, "\n\n"));
        else if (!sect_node->next->tk.value.starts_with("\n\n")) {
            sect_node->next->tk.value.insert(0, "\n");
            if (!sect_node->next->tk.value.starts_with("\n\n"))
                sect_node->next->tk.value.insert(0, "\n");
        }

        return new_section;
    }

    [[nodiscard]]
    bool operator==(const cfg& rhs) const {
        for (auto& [sect_name, sect] : sections) {
            auto found = rhs.sections.find(sect_name);
            if (found == rhs.sections.end())
                return false;

            if (sect != found->second)
                return false;
        }
        return true;
    }

    [[nodiscard]]
    bool operator!=(const cfg& rhs) const {
        return !(*this == rhs);
    }

private:
    [[nodiscard]]
    cfg_node* calc_tail() const {
        auto ptr = head.get();
        while (ptr->next)
            ptr = ptr->next.get();
        return ptr;
    }

    void parse(const fs::path& config_path, cfg_node*& tail) {
        auto path = fs::weakly_canonical(config_path);
        auto str_path = path.string();

        auto [pos, was_insert] = file_nodes.emplace(str_path, nullptr);
        if (!was_insert)
            throw cfg_already_parsed(str_path);
        auto& file_node = pos->second;

        auto mfv       = mmap_file_view(str_path.data());
        auto tokenizer = cfg_tokenizer(mfv.begin(), mfv.end());

        while (tokenizer) {
            auto [type, tk] = tokenizer.next();
            tail = tail->insert_after(cfg_node::create(type, std::move(tk)), false);

            if (!file_node) {
                file_node = std::make_unique<cfg_file_node>(str_path, tail);
                file_stack.push_back(file_node.get());
                for (auto f : file_stack)
                    file_node->inner.insert(f);
            }

            tail->file = file_node.get();

            switch (type) {
            case cfg_token_t::sect_declaration:
                insert_next();
                {
                    auto& sectname = tail->tk.value;
                    auto [pos, was_insert] =
                        sections.emplace(sectname.substr(1, sectname.size() - 2), tail);
                    if (!was_insert)
                        throw cfg_section_already_exists(pos->first);
                    current_section = &pos->second;
                }
                break;
            case cfg_token_t::include_macro:
                insert_next();
                parse(config_path.parent_path() / parse_include(tail->tk.value), tail);
                break;
            case cfg_token_t::key:
                /* Handle case when previous key has not value */
                if (current_key)
                    insert_next();

                current_key = tail;
                break;
            case cfg_token_t::eq:
                current_eq = tail;
                break;
            case cfg_token_t::value:
                current_value = tail;
                insert_next();
                break;
            default: break;
            }
        }

        file_stack.pop_back();
        insert_next();
    }

    /* Inserts the next key/value pair */
    void insert_next() {
        if (!current_key)
            return;

        if (!current_section)
            current_section =
                &sections.emplace(std::string(), cfg_section<false>(head.get())).first->second;

        if (!current_eq)
            throw cfg_key_without_eq(current_section->section_name(), current_key->tk.value);

        auto was_insert =
            current_section->values
                .emplace(cfg_key{current_key}, current_value ? current_value : current_eq)
                .second;

        if (!was_insert)
            throw cfg_key_already_exists(current_section->section_name(), current_key->tk.value);

        current_key   = nullptr;
        current_eq    = nullptr;
        current_value = nullptr;
    }

private:
    std::map<std::string, std::unique_ptr<cfg_file_node>> file_nodes;
    std::map<std::string, cfg_section<false>>             sections;
    std::unique_ptr<cfg_node>                             head;
    std::vector<cfg_file_node*>                           file_stack;

    cfg_section<false>* current_section = nullptr;
    cfg_node*           current_key     = nullptr;
    cfg_node*           current_value   = nullptr;
    cfg_node*           current_eq      = nullptr;
};

} // namespace dfdh
