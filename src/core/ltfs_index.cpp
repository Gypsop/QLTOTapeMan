#include "ltfs_index.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace qlto {

namespace {

inline void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));
}

inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
}

inline void trim(std::string &s) {
    ltrim(s);
    rtrim(s);
}

std::string load_file(const std::string &path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
        throw std::runtime_error("cannot open schema: " + path);
    }
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

void save_file(const std::string &path, const std::string &content) {
    std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
    if (!ofs) {
        throw std::runtime_error("cannot write schema: " + path);
    }
    ofs << content;
}

struct Cursor {
    const std::string &s;
    std::size_t pos{0};
};

void skip_ws(Cursor &c) {
    while (c.pos < c.s.size() && std::isspace(static_cast<unsigned char>(c.s[c.pos]))) {
        ++c.pos;
    }
}

bool starts_with(const std::string &s, std::size_t pos, const std::string &prefix) {
    return s.compare(pos, prefix.size(), prefix) == 0;
}

std::string parse_tag(Cursor &c) {
    if (c.s[c.pos] != '<') return {};
    auto end = c.s.find('>', c.pos);
    if (end == std::string::npos) return {};
    std::string tag = c.s.substr(c.pos + 1, end - c.pos - 1);
    c.pos = end + 1;
    return tag;
}

std::vector<std::string> split_ws(const std::string &s) {
    std::vector<std::string> out;
    std::string cur;
    std::istringstream iss(s);
    while (iss >> cur) out.push_back(cur);
    return out;
}

std::string unquote(const std::string &v) {
    if (v.size() >= 2 && v.front() == '"' && v.back() == '"') {
        return v.substr(1, v.size() - 2);
    }
    return v;
}

std::unordered_map<std::string, std::string> parse_attrs(const std::string &tag) {
    std::unordered_map<std::string, std::string> m;
    auto parts = split_ws(tag);
    if (parts.empty()) return m;
    for (std::size_t i = 1; i < parts.size(); ++i) {
        auto &p = parts[i];
        auto eq = p.find('=');
        if (eq == std::string::npos) continue;
        auto k = p.substr(0, eq);
        auto v = unquote(p.substr(eq + 1));
        m[k] = v;
    }
    return m;
}

LtfsFile parse_file(const std::string &tag, Cursor &c) {
    LtfsFile f{};
    auto attrs = parse_attrs(tag);
    f.name = attrs["name"];
    if (attrs.count("fileuid")) f.fileuid = std::stoll(attrs["fileuid"]);
    if (attrs.count("length")) f.length = static_cast<std::uint64_t>(std::stoull(attrs["length"]));
    if (attrs.count("readonly")) f.readonly = (attrs["readonly"] == "1" || attrs["readonly"] == "true");
    if (attrs.count("openforwrite")) f.openforwrite = (attrs["openforwrite"] == "1" || attrs["openforwrite"] == "true");
    if (attrs.count("selected")) f.selected = (attrs["selected"] == "1" || attrs["selected"] == "true");

    if (attrs.count("creationtime")) f.creationtime = attrs["creationtime"];
    if (attrs.count("modifytime")) f.modifytime = attrs["modifytime"];
    if (attrs.count("accesstime")) f.accesstime = attrs["accesstime"];
    if (attrs.count("changetime")) f.changetime = attrs["changetime"];
    if (attrs.count("backuptime")) f.backuptime = attrs["backuptime"];

    bool self_closed = tag.size() >= 1 && tag[tag.size() - 1] == '/';
    if (self_closed) return f;

    // parse nested xattr until </file>
    while (c.pos < c.s.size()) {
        skip_ws(c);
        if (starts_with(c.s, c.pos, "</file")) {
            auto endtag = parse_tag(c);
            (void)endtag;
            break;
        }
        if (c.pos >= c.s.size()) break;
        if (c.s[c.pos] != '<') { ++c.pos; continue; }
        auto inner = parse_tag(c);
        if (inner.rfind("xattr", 0) == 0) {
            auto a2 = parse_attrs(inner);
            LtfsXAttr xa;
            xa.name = a2["name"];
            xa.value = a2["value"];
            f.extendedattributes.push_back(std::move(xa));
            // xattr assumed self-closing
        }
    }
    return f;
}

void parse_directory(Cursor &c, LtfsDirectory &dir, const std::string &tag) {
    auto attrs = parse_attrs(tag);
    if (attrs.count("name")) dir.name = attrs["name"];
    if (attrs.count("selected")) dir.selected = (attrs["selected"] == "1" || attrs["selected"] == "true");

    bool self_closed = tag.size() >= 1 && tag[tag.size() - 1] == '/';
    if (self_closed) return;

    while (c.pos < c.s.size()) {
        skip_ws(c);
        if (starts_with(c.s, c.pos, "</directory")) {
            auto endtag = parse_tag(c);
            (void)endtag;
            break;
        }
        if (c.pos >= c.s.size()) break;
        if (c.s[c.pos] != '<') { ++c.pos; continue; }
        auto inner = parse_tag(c);
        if (inner.rfind("directory", 0) == 0) {
            LtfsDirectory child;
            parse_directory(c, child, inner);
            dir.contents.directories.push_back(std::move(child));
        } else if (inner.rfind("file", 0) == 0) {
            auto f = parse_file(inner, c);
            dir.contents.files.push_back(std::move(f));
        }
    }
}

std::string escape_xml(const std::string &s) {
    std::string out;
    out.reserve(s.size());
    for (char ch : s) {
        switch (ch) {
        case '&': out += "&amp;"; break;
        case '"': out += "&quot;"; break;
        case '\'': out += "&apos;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        default: out.push_back(ch); break;
        }
    }
    return out;
}

void write_directory(std::ostringstream &oss, const LtfsDirectory &dir, int indent) {
    auto pad = std::string(indent, ' ');
    oss << pad << "<directory name=\"" << escape_xml(dir.name) << "\" selected=\"" << (dir.selected ? "1" : "0") << "\">\n";
    for (const auto &child : dir.contents.directories) {
        write_directory(oss, child, indent + 2);
    }
    for (const auto &f : dir.contents.files) {
        oss << pad << "  <file name=\"" << escape_xml(f.name) << "\" length=\"" << f.length << "\" fileuid=\"" << f.fileuid
            << "\" readonly=\"" << (f.readonly ? "1" : "0") << "\" openforwrite=\"" << (f.openforwrite ? "1" : "0")
            << "\" selected=\"" << (f.selected ? "1" : "0") << "\" creationtime=\"" << escape_xml(f.creationtime)
            << "\" modifytime=\"" << escape_xml(f.modifytime) << "\" accesstime=\"" << escape_xml(f.accesstime)
            << "\" changetime=\"" << escape_xml(f.changetime) << "\" backuptime=\"" << escape_xml(f.backuptime) << "\"";
        if (f.extendedattributes.empty()) {
            oss << "/>\n";
        } else {
            oss << ">\n";
            for (const auto &xa : f.extendedattributes) {
                oss << pad << "    <xattr name=\"" << escape_xml(xa.name) << "\" value=\"" << escape_xml(xa.value) << "\"/>\n";
            }
            oss << pad << "  </file>\n";
        }
    }
    oss << pad << "</directory>\n";
}

} // namespace

bool load_schema(const std::string &path, LtfsIndex &out, std::string &err) {
    try {
        std::string text = load_file(path);
        Cursor c{text, 0};
        skip_ws(c);
        if (c.pos >= c.s.size() || c.s[c.pos] != '<') {
            err = "invalid schema: missing root";
            return false;
        }
        auto root_tag = parse_tag(c);
        if (root_tag.rfind("ltfsindex", 0) != 0) {
            err = "invalid schema: missing ltfsindex";
            return false;
        }
        auto attrs = parse_attrs(root_tag);
        if (attrs.count("partition")) out.location.partition = static_cast<std::uint8_t>(std::stoi(attrs["partition"]));
        if (attrs.count("startblock")) out.location.startblock = static_cast<std::uint64_t>(std::stoull(attrs["startblock"]));
        if (attrs.count("generationnumber")) out.generationnumber = static_cast<std::uint32_t>(std::stoul(attrs["generationnumber"]));
        if (attrs.count("volumeuuid")) out.volumeuuid = attrs["volumeuuid"];

        skip_ws(c);
        if (!starts_with(c.s, c.pos, "<directory")) {
            err = "invalid schema: missing root directory";
            return false;
        }
        auto dir_tag = parse_tag(c);
        parse_directory(c, out.root, dir_tag);
        return true;
    } catch (const std::exception &ex) {
        err = ex.what();
        return false;
    }
}

bool save_schema(const LtfsIndex &idx, const std::string &path, std::string &err) {
    try {
        std::ostringstream oss;
        oss << "<ltfsindex partition=\"" << static_cast<int>(idx.location.partition)
            << "\" startblock=\"" << idx.location.startblock
            << "\" generationnumber=\"" << idx.generationnumber
            << "\" volumeuuid=\"" << escape_xml(idx.volumeuuid) << "\">\n";
        write_directory(oss, idx.root, 2);
        oss << "</ltfsindex>\n";
        save_file(path, oss.str());
        return true;
    } catch (const std::exception &ex) {
        err = ex.what();
        return false;
    }
}

void select_all(LtfsDirectory &dir, bool selected) {
    dir.selected = selected;
    for (auto &d : dir.contents.directories) {
        select_all(d, selected);
    }
    for (auto &f : dir.contents.files) {
        f.selected = selected;
    }
}

void select_by_predicate(LtfsDirectory &dir, const std::function<bool(const LtfsFile &)> &pred) {
    for (auto &f : dir.contents.files) {
        f.selected = pred ? pred(f) : f.selected;
    }
    for (auto &d : dir.contents.directories) {
        select_by_predicate(d, pred);
        // directory selected if any child selected
        bool child_sel = d.selected;
        for (const auto &cf : d.contents.files) child_sel = child_sel || cf.selected;
        for (const auto &cd : d.contents.directories) child_sel = child_sel || cd.selected;
        d.selected = child_sel;
    }
}

void sort_files(LtfsDirectory &dir, const std::function<bool(const LtfsFile &, const LtfsFile &)> &cmp) {
    if (cmp) {
        std::sort(dir.contents.files.begin(), dir.contents.files.end(), cmp);
    }
    for (auto &d : dir.contents.directories) {
        sort_files(d, cmp);
    }
}

std::string sanitize_filename(const std::string &name) {
    std::string out;
    out.reserve(name.size());
    const std::string invalid = "\\/:*?\"<>|";
    for (char ch : name) {
        if (std::iscntrl(static_cast<unsigned char>(ch)) || invalid.find(ch) != std::string::npos) {
            out.push_back('_');
        } else {
            out.push_back(ch);
        }
    }
    if (out.empty()) out = "_";
    return out;
}

} // namespace qlto
