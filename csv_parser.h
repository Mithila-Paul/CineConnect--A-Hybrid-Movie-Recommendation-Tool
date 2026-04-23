#ifndef CSV_PARSER_H
#define CSV_PARSER_H

#include <string>
#include <vector>
#include <sstream>

inline std::string parseCsvField(std::istream &ss)
{
    std::string field;

    if (ss.peek() == '"')
    {
        ss.get(); // skip opening quote

        char c;
        while (ss.get(c))
        {
            if (c == '"')
            {
                if (ss.peek() == '"')
                {
                    ss.get();      // skip escaped quote
                    field += '"';  // keep one quote
                }
                else
                {
                    break; // end of quoted field
                }
            }
            else
            {
                field += c;
            }
        }

        if (ss.peek() == ',')
        {
            ss.get(); // skip comma after quoted field
        }
    }
    else
    {
        std::getline(ss, field, ',');
    }

    return field;
}

inline std::vector<std::string> parseCsvRow(const std::string &line)
{
    std::vector<std::string> fields;
    std::istringstream ss(line);

    while (true)
    {
        if (ss.peek() == EOF)
        {
            break;
        }

        fields.push_back(parseCsvField(ss));

        if (ss.eof())
        {
            break;
        }
    }

    return fields;
}

inline void stripCR(std::string &line)
{
    if (!line.empty() && line[line.size() - 1] == '\r')
    {
        line.erase(line.size() - 1);
    }
}

#endif