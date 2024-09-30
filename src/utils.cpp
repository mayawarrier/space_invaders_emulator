
#include "utils.hpp"

static std::string_view trim_v(std::string_view str)
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

    str = trim_v({ src.data(), endpos });
    src.remove_prefix(nread);
    return true;
}

template <typename TString>
static bool getline_v(std::string_view& src, TString& line)
{
    return extract_str(src, line, '\n');
}

void ini::set_value_internal(const std::string& section,
    const std::string& key, std::string_view value)
{
    // add indexes to keys, so data can be written back in same order

    auto sectitr = m_map.find(section);
    if (sectitr != m_map.end())
    {
        auto keyitr = sectitr->second.find(key);
        if (keyitr != sectitr->second.end()) {
            keyitr->second = value;
        }
        else {
            auto fullkey = std::to_string(sectitr->second.size());
            fullkey += "_"; fullkey += key;
            sectitr->second[std::move(fullkey)] = value;
        }
    }
    else {
        std::string fullsect = std::to_string(m_map.size());
        fullsect += "_"; fullsect += section;

        std::string fullkey = "0_"; fullkey += key;
        m_map[std::move(fullsect)][std::move(fullkey)] = value;
    }
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
    std::string_view line;
    std::string cur_section;
    std::string_view filedata(filebuf.get(), filesize);

    while (getline_v(filedata, line))
    {
        if (line.empty()) {
            continue;
        }
        if (line.starts_with('[') && line.ends_with(']'))
        {
            cur_section = line.substr(1, line.length() - 2);
            if (m_map.contains(cur_section)) {
                return logERROR("%s: Duplicate section on line %d", m_path.string().c_str(), lineno);
            }
        }
        else {
            std::string key;
            std::string_view value;
            if (!extract_str(line, key, '=') || key.empty() ||
                !extract_str(line, value, '\n') || value.empty()) {
                return logERROR("%s: Invalid entry on line %d", m_path.string().c_str(), lineno);
            }

            set_value_internal(cur_section, key, value);
        }
        lineno++;
    }

    logMESSAGE("Read prefs");
    return 0;
}

static std::pair<std::string_view, std::string_view> idx_and_key(const std::string& str)
{
    size_t off = str.find('_');
    return { {str.begin(), str.begin() + off }, { str.begin() + off + 1, str.end() } };
}

int ini::write()
{
    std::string out;

    struct section {
        std::string_view name;
        struct kv {
            std::string_view key, value;
        };
        std::vector<kv> entries;
    };
    std::vector<section> m_data;
    m_data.resize(m_map.size());

    for (auto it = m_map.begin(); it != m_map.end(); ++it)
    {
        // add section
        auto [idxstr, section] = idx_and_key(it->first);
        size_t sectidx = 0;
        try_parse_num(idxstr, sectidx);
        m_data[sectidx].name = section;
        m_data[sectidx].entries.resize(it->second.size());
       
        // add section keys
        for (auto keyit = it->second.begin(); keyit != it->second.end(); ++keyit) 
        {
            auto [keyidxstr, key] = idx_and_key(keyit->first);
            size_t keyidx = 0;
            try_parse_num(keyidxstr, keyidx);

            auto& entry = m_data[sectidx].entries[keyidx];
            entry.key = key;
            entry.value = keyit->second;
        }
    }

    for (int i = 0; i < m_data.size(); ++i)
    {
        auto& cur_section = m_data[i].name;
        out += "["; out += cur_section; out += "]\n";
    
        for (int j = 0; j < m_data[i].entries.size(); ++j)
        {
            auto& entry = m_data[i].entries[j];
            out += entry.key;
            out += " = ";
            out += entry.value;
            out += "\n";
        }

        out += "\n";
    }

    scopedFILE file = SAFE_FOPEN(m_path.c_str(), "wb");
    if (!file) {
        return logERROR("Could not open file %s", m_path.string().c_str());
    }
    if (std::fwrite(out.c_str(), 1, out.length(), file.get()) != out.length()) {
        return logERROR("Could not write to file %s", m_path.string().c_str());
    }

    return 0;
}
