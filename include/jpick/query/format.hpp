// format.hpp — output format filters (@base64, @base32, @uri, @html, @sh,
// @csv, @tsv, @json, @text) and raw_value, the raw rendering used by string
// interpolation. Included via query.hpp.
#pragma once

#include <string>
#include <vector>
#include <stdexcept>
#include "jpick/json.hpp"
#include "jpick/serializer.hpp"

namespace jpick
{
    // Render a value the way string interpolation expects it: a string is
    // emitted raw (without surrounding quotes), everything else is serialized.
    inline std::string raw_value(const Value &value)
    {
        if (value.is_string())
            return value.as_string();
        return serialize(value);
    }

    // RFC 4648 base64 encoding of an arbitrary byte string. Uses a bit
    // accumulator symmetric with base64_decode: every input byte adds 8 bits
    // and each full group of 6 bits emits one character; leftover bits are
    // zero-padded and the output is padded with '=' to a multiple of four.
    inline std::string base64_encode(const std::string &in)
    {
        static const char table[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string out;
        out.reserve(((in.size() + 2) / 3) * 4);
        unsigned buffer = 0;
        int bits = 0;
        for (unsigned char c : in)
        {
            buffer = (buffer << 8) | c;
            bits += 8;
            while (bits >= 6)
            {
                bits -= 6;
                out += table[(buffer >> bits) & 63];
            }
        }
        if (bits > 0)
            out += table[(buffer << (6 - bits)) & 63];
        while (out.size() % 4 != 0)
            out += '=';
        return out;
    }

    // RFC 4648 base64 decoding. Whitespace is ignored, '=' ends the data,
    // any other character is rejected. Uses a 6-bit accumulator: every input
    // character adds 6 bits and each full group of 8 bits emits one byte.
    inline std::string base64_decode(const std::string &in)
    {
        const auto sextet = [](char c) -> int
        {
            if (c >= 'A' && c <= 'Z')
                return c - 'A';
            if (c >= 'a' && c <= 'z')
                return c - 'a' + 26;
            if (c >= '0' && c <= '9')
                return c - '0' + 52;
            if (c == '+')
                return 62;
            if (c == '/')
                return 63;
            return -1;
        };

        std::string out;
        unsigned buffer = 0;
        int bits = 0;
        for (char c : in)
        {
            if (c == '=')
                break;
            if (c == '\n' || c == '\r' || c == '\t' || c == ' ')
                continue;
            const int value = sextet(c);
            if (value < 0)
                throw std::runtime_error("Invalid base64 input");
            buffer = (buffer << 6) | static_cast<unsigned>(value);
            bits += 6;
            if (bits >= 8)
            {
                bits -= 8;
                out += static_cast<char>((buffer >> bits) & 0xFF);
            }
        }
        return out;
    }

    // RFC 4648 base32 encoding. A bit accumulator emits one character per 5
    // bits; leftover bits are zero-padded and the output is padded with '=' to
    // a multiple of eight.
    inline std::string base32_encode(const std::string &in)
    {
        static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
        std::string out;
        unsigned buffer = 0;
        int bits = 0;
        for (unsigned char c : in)
        {
            buffer = (buffer << 8) | c;
            bits += 8;
            while (bits >= 5)
            {
                bits -= 5;
                out += table[(buffer >> bits) & 31];
            }
        }
        if (bits > 0)
            out += table[(buffer << (5 - bits)) & 31];
        while (out.size() % 8 != 0)
            out += '=';
        return out;
    }

    // RFC 4648 base32 decoding. Whitespace is ignored, '=' ends the data, and
    // any other character is rejected (lowercase letters are accepted). A 5-bit
    // accumulator emits one byte per full group of 8 bits.
    inline std::string base32_decode(const std::string &in)
    {
        const auto quintet = [](char c) -> int
        {
            if (c >= 'A' && c <= 'Z')
                return c - 'A';
            if (c >= 'a' && c <= 'z')
                return c - 'a';
            if (c >= '2' && c <= '7')
                return c - '2' + 26;
            return -1;
        };

        std::string out;
        unsigned buffer = 0;
        int bits = 0;
        for (char c : in)
        {
            if (c == '=')
                break;
            if (c == '\n' || c == '\r' || c == '\t' || c == ' ')
                continue;
            const int value = quintet(c);
            if (value < 0)
                throw std::runtime_error("Invalid base32 input");
            buffer = (buffer << 5) | static_cast<unsigned>(value);
            bits += 5;
            if (bits >= 8)
            {
                bits -= 8;
                out += static_cast<char>((buffer >> bits) & 0xFF);
            }
        }
        return out;
    }

    // Percent-encode a string for use in a URI (RFC 3986). The unreserved
    // characters A-Z a-z 0-9 - _ . ~ are kept; everything else becomes %XX.
    inline std::string uri_encode(const std::string &in)
    {
        static const char hex[] = "0123456789ABCDEF";
        std::string out;
        for (char c : in)
        {
            const unsigned char uc = static_cast<unsigned char>(c);
            if ((uc >= 'A' && uc <= 'Z') || (uc >= 'a' && uc <= 'z') ||
                (uc >= '0' && uc <= '9') || uc == '-' || uc == '_' ||
                uc == '.' || uc == '~')
            {
                out += c;
            }
            else
            {
                out += '%';
                out += hex[uc >> 4];
                out += hex[uc & 0x0F];
            }
        }
        return out;
    }

    // Escape a string for safe inclusion in HTML (like jq's @html): the
    // characters & < > ' " become their entity references.
    inline std::string html_escape(const std::string &in)
    {
        std::string out;
        for (char c : in)
        {
            switch (c)
            {
            case '&':
                out += "&amp;";
                break;
            case '<':
                out += "&lt;";
                break;
            case '>':
                out += "&gt;";
                break;
            case '\'':
                out += "&#39;";
                break;
            case '"':
                out += "&quot;";
                break;
            default:
                out += c;
                break;
            }
        }
        return out;
    }

    // Join the elements of an array into one line, rendering each with `field`
    // and separating them with `sep`. Shared by @sh, @csv and @tsv.
    inline std::string join_row(const Array &arr, const std::string &sep,
                                std::string (*field)(const Value &))
    {
        std::string out;
        for (std::size_t i = 0; i < arr.size(); ++i)
        {
            if (i > 0)
                out += sep;
            out += field(arr[i]);
        }
        return out;
    }

    // Escape a single scalar for a POSIX shell: strings are single-quoted
    // (inner ' becomes '\''), numbers and bools are emitted as-is. null,
    // arrays and objects cannot be escaped this way.
    inline std::string sh_scalar(const Value &value)
    {
        if (value.is_string())
        {
            std::string out = "'";
            for (char c : value.as_string())
            {
                if (c == '\'')
                    out += "'\\''";
                else
                    out += c;
            }
            out += "'";
            return out;
        }
        if (value.is_bool() || value.is_number())
            return serialize(value);
        throw std::runtime_error("Cannot escape this value for the shell");
    }

    // @sh over an array escapes each element and joins them with spaces;
    // over a single scalar it escapes just that value.
    inline std::string sh_format(const Value &value)
    {
        if (value.is_array())
            return join_row(value.as_array(), " ", sh_scalar);
        return sh_scalar(value);
    }

    // Render one field of an @csv / @tsv row. Scalars only: strings are
    // escaped, null becomes an empty field, arrays/objects are rejected.
    inline std::string csv_field(const Value &value)
    {
        if (value.is_string())
        {
            std::string out = "\"";
            for (char c : value.as_string())
            {
                if (c == '"')
                    out += "\"\"";
                else
                    out += c;
            }
            out += "\"";
            return out;
        }
        if (value.is_null())
            return "";
        if (value.is_bool() || value.is_number())
            return serialize(value);
        throw std::runtime_error("Cannot format an array or object as CSV");
    }

    inline std::string tsv_field(const Value &value)
    {
        if (value.is_string())
        {
            std::string out;
            for (char c : value.as_string())
            {
                switch (c)
                {
                case '\t':
                    out += "\\t";
                    break;
                case '\n':
                    out += "\\n";
                    break;
                case '\r':
                    out += "\\r";
                    break;
                case '\\':
                    out += "\\\\";
                    break;
                default:
                    out += c;
                    break;
                }
            }
            return out;
        }
        if (value.is_null())
            return "";
        if (value.is_bool() || value.is_number())
            return serialize(value);
        throw std::runtime_error("Cannot format an array or object as TSV");
    }

    // Apply a @format segment (e.g. @csv) to a single value, producing a
    // string value. @csv/@tsv expect an array of scalars.
    inline Value apply_format(const std::string &fmt, const Value &value)
    {
        if (fmt == "@text")
            return Value(raw_value(value));
        if (fmt == "@json")
            return Value(serialize(value));
        if (fmt == "@base64")
            return Value(base64_encode(raw_value(value)));
        if (fmt == "@base64d")
            return Value(base64_decode(raw_value(value)));
        if (fmt == "@base32")
            return Value(base32_encode(raw_value(value)));
        if (fmt == "@base32d")
            return Value(base32_decode(raw_value(value)));
        if (fmt == "@uri")
            return Value(uri_encode(raw_value(value)));
        if (fmt == "@html")
            return Value(html_escape(raw_value(value)));
        if (fmt == "@sh")
            return Value(sh_format(value));
        if (fmt == "@csv")
            return Value(join_row(value.as_array(), ",", csv_field));
        if (fmt == "@tsv")
            return Value(join_row(value.as_array(), "\t", tsv_field));
        throw std::runtime_error("Unknown format: " + fmt);
    }
} // namespace jpick
