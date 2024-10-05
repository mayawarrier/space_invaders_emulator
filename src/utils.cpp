
#include "utils.hpp"

static std::string_view trim(std::string_view str)
{
    const char* beg = str.data();
    const char* end = str.data() + str.length();

    while (beg != end && is_ws(*beg)) { ++beg; }
    while (end != beg && is_ws(*(end - 1))) { --end; }
  
    return { beg, end };
}

template <typename TString>
static bool extract_str(std::string_view& src, TString& str, char delim)
{
    if (src.size() == 0) { return false; }

    size_t i = src.find(delim);
    size_t endpos = (i != src.npos) ? i : src.size();
    size_t nread = (i != src.npos) ? i + 1 : src.size(); // skip delim

    str = trim({ src.data(), endpos });
    src.remove_prefix(nread);
    return true;
}

template <typename TString>
static bool getline_v(std::string_view& src, TString& line)
{
    return extract_str(src, line, '\n');
}

void ini::set_value_internal(std::string_view section,
    std::string_view key, std::string_view value)
{
    auto sectitr = m_map.insert({ 
        std::string(section), 
        mapsection(m_map.size()) }).first;

    auto& sectentries = sectitr->second.entries;
    auto keyitr = sectentries.insert({ 
        std::string(key),
        mapsection::value(sectentries.size()) }).first;

    keyitr->second.valuestr = value;
}

int ini::read()
{
    auto filesize = size_t(fs::file_size(m_path));
    auto filebuf = std::make_unique<char[]>(filesize);
    {
        scopedFILE file = SAFE_FOPEN(m_path.c_str(), "rb");
        if (!file) {
            return logERROR("Could not open file %s", m_path.string().c_str());
        }
        if (std::fread(filebuf.get(), 1, filesize, file.get()) != filesize) {
            return logERROR("Could not read from file %s", m_path.string().c_str());
        }
    }

    int lineno = 1;
    std::string_view line, section;
    std::string_view filedata(filebuf.get(), filesize);

    while (getline_v(filedata, line))
    {
        if (line.empty()) {
            continue;
        }
        if (line.starts_with('[') && line.ends_with(']')) {
            section = line.substr(1, line.length() - 2);
        } 
        else {
            std::string_view key, value;
            if (!extract_str(line, key, '=') || key.empty() ||
                !extract_str(line, value, '\n') || value.empty()) {
                return logERROR("%s: Invalid entry on line %d", path_str().c_str(), lineno);
            }
            set_value_internal(section, key, value);
        }            
        lineno++;
    }

    return 0;
}

int ini::write()
{
    struct vecsection
    {
        using kv = std::pair<std::string_view, std::string_view>;
        std::string_view name;
        std::vector<kv> entries;
    };
    std::vector<vecsection> data(m_map.size());

    for (const auto& [sectname, sect] : m_map)
    {
        auto& vecsect = data[sect.index];
        vecsect = { .name = sectname };
        vecsect.entries.resize(sect.entries.size());

        for (const auto& [key, value] : sect.entries) {
            vecsect.entries[value.index] = { key, value.valuestr };
        }
    }

    std::string out;
    for (int i = 0; i < data.size(); ++i)
    {
        auto& section = data[i].name;
        out += "["; out += section; out += "]\n";
    
        for (int j = 0; j < data[i].entries.size(); ++j)
        {
            auto& entry = data[i].entries[j];
            out += entry.first;
            out += " = ";
            out += entry.second;
            out += "\n";
        }
        out += "\n";
    }

    scopedFILE file = SAFE_FOPEN(m_path.c_str(), "wb");
    if (!file) {
        return logERROR("Could not open file %s", path_str().c_str());
    }
    if (std::fwrite(out.c_str(), 1, out.length(), file.get()) != out.length()) {
        return logERROR("Could not write to file %s", path_str().c_str());
    }

    return 0;
}
