#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace BmcvJson {
using namespace std;

class CCharArray {
   public:
    CCharArray(const char *raw_pointer, uint32_t start_pos, uint32_t end_pos)
        : start(start_pos), end(end_pos), raw(raw_pointer){};
    CCharArray() : start(0), end(0), raw(nullptr){};
    CCharArray(const char *raw_pointer)
        : start(0), end(strlen(raw_pointer)), raw(raw_pointer){};
    friend ostream &operator<<(ostream &os, const CCharArray &s);
    CCharArray      substr(uint32_t start);
    CCharArray      substr(uint32_t start, uint32_t len);
    string          tostr();
    uint32_t        len();
    char            pop_front();
    CCharArray      strip();
    char            operator[](uint32_t index) const;

   private:
    uint32_t    start;
    uint32_t    end;
    const char *raw;
};

char CCharArray::pop_front() {
    if (this->len() <= 0)
        return EOF;
    char res = (*this)[0];
    start++;
    return res;
}

CCharArray CCharArray::strip() {
    uint32_t start;
    int      end;
    if (this->len() == 0)
        return *this;
    for (start = 0; start < this->len(); start++) {
        if (!isspace((*this)[start]))
            break;
    }
    for (end = (int)this->len() - 1; end >= 0; end--) {
        if (!isspace((*this)[end])) {
            end++;
            break;
        }
    }
    int len = end - start;
    if (len < 0)
        return CCharArray();
    return this->substr(start, len);
}

ostream &operator<<(ostream &os, const CCharArray &s) {
    // any good way here?
    uint32_t len = s.end - s.start;
    for (uint32_t i = 0; i < len; i++) {
        os << s[i];
    }
    return os;
}

string CCharArray::tostr() {
    ostringstream os;
    os << *this;
    return os.str();
}

uint32_t CCharArray::len() {
    return std::max<uint32_t>(end - start, 0);
}

char CCharArray::operator[](uint32_t index) const {
    assert(index < end - start);
    return raw[start + index];
}

CCharArray CCharArray::substr(uint32_t start_pos, uint32_t len) {
    assert(start_pos < end);
    assert(start + start_pos + len <= end);
    return CCharArray(raw, start + start_pos, start + start_pos + len);
}

CCharArray CCharArray::substr(uint32_t start_pos) {
    return substr(start_pos, len() - start_pos);
}

typedef enum {
    JSON_UNKNOWN = 0,
    JSON_STRING,
    JSON_NUMBER,
    JSON_NULL,
    JSON_BOOLEAN,
    JSON_OBJECT,
    JSON_ARRAY,
    JSON_EMPTY
} JsonType;

struct JsonBase {
    JsonType       type;
    virtual string visit() = 0;
    void *         get() {
        return (void *)(this);
    };
    bool operator<(JsonBase &b) {
        return get() < b.get();
    };
};
#define DEFINE_JSON_TYPE(classname, value_type, json_type)                     \
    struct classname : JsonBase {                                              \
        value_type     value;                                                  \
        virtual string visit();                                                \
        classname() {                                                          \
            type = json_type;                                                  \
        };                                                                     \
        classname(value_type value_) {                                         \
            type  = json_type;                                                 \
            value = value_;                                                    \
        };                                                                     \
    }

using jsonmap = vector<pair<std::string, shared_ptr<JsonBase>>>;

DEFINE_JSON_TYPE(JsonString, string, JSON_STRING);
DEFINE_JSON_TYPE(JsonNumber, double, JSON_NUMBER);
DEFINE_JSON_TYPE(JsonNull, string, JSON_NULL);
DEFINE_JSON_TYPE(JsonBoolean, bool, JSON_BOOLEAN);
// DEFINE_JSON_TYPE(JsonObject, jsonmap, JSON_OBJECT);
DEFINE_JSON_TYPE(JsonArray, vector<shared_ptr<JsonBase>>, JSON_ARRAY);

struct JsonObject : JsonBase {
    jsonmap             value;
    virtual std::string visit();
    JsonObject() {
        type = JSON_OBJECT;
    };
    shared_ptr<JsonBase> &operator[](std::string);
    shared_ptr<JsonBase>  operator[](std::string) const;
};

shared_ptr<JsonBase> &JsonObject::operator[](std::string key) {
    for (auto &&it : value) {
        if (it.first == key)
            return it.second;
    }
    shared_ptr<JsonObject> base = std::make_shared<JsonObject>();
    std::pair<std::string, shared_ptr<JsonBase>> kv(key, std::move(base));
    value.push_back(kv);
    auto it = value.end();
    it--;
    return it->second;
}

shared_ptr<JsonBase> JsonObject::operator[](std::string key) const {
    for (auto &&it : value) {
        if (it.first == key)
            return it.second;
    }
    assert(0);
    shared_ptr<JsonObject> base;
    return base;
}

string JsonString::visit() {
    return "\"" + value + "\"";
}

string JsonNumber::visit() {
    ostringstream os;
    os << value;
    return os.str();
}

string JsonNull::visit() {
    return "null";
}

string JsonBoolean::visit() {
    if (value)
        return "true";
    else
        return "false";
}

string JsonObject::visit() {
    string res = "{ ";
    for (auto it = value.begin(); it != value.end(); it++) {
        string k = it->first;
        auto   v = it->second;
        res += "\"" + k + "\"";
        res += " : ";
        res += v->visit();
        if (std::next(it) != value.end())
            res += ", ";
    }
    res += "}\n";
    return res;
}

string JsonArray::visit() {
    string res = "[ ";
    for (auto it = value.begin(); it != value.end(); it++) {
        res += (*it)->visit();
        if (std::next(it) != value.end())
            res += ", ";
    }
    res += "]\n";
    return res;
}

class JsonParser {
   public:
    JsonObject *parse(const char *s);

   protected:
    shared_ptr<JsonBase> parse_object(CCharArray &s);
    shared_ptr<JsonBase> parse_string(CCharArray &s);
    shared_ptr<JsonBase> parse_null(CCharArray &s);
    shared_ptr<JsonBase> parse_bool(CCharArray &s);
    shared_ptr<JsonBase> parse_array(CCharArray &s);
    shared_ptr<JsonBase> parse_number(CCharArray &s);

   private:
    shared_ptr<JsonBase> root;
    CCharArray           raw_input;
    char                 current_char;
};

JsonObject *JsonParser::parse(const char *s) {
    raw_input = CCharArray(s);
    raw_input = raw_input.strip();
    root.reset();

    if (raw_input.len() == 0)
        return dynamic_cast<JsonObject *>(root.get());
    // First one should be object
    current_char = raw_input.pop_front();
    assert(current_char == '{');
    root = parse_object(raw_input);
    return dynamic_cast<JsonObject *>(root.get());
}

shared_ptr<JsonBase> JsonParser::parse_object(CCharArray &s) {
    JsonObject *res = new JsonObject;
    assert(current_char == '{');
    s            = s.strip();
    current_char = s.pop_front();
    if (current_char == '}') {
        return shared_ptr<JsonBase>(res);
    }

    while (current_char != EOF) {
        assert(current_char == '\"');
        string key = (dynamic_cast<JsonString *>(parse_string(s).get()))->value;
        shared_ptr<JsonBase> value;
        while (isspace(current_char) && s.len() > 0) {
            current_char = s.pop_front();
        }
        assert(current_char == ':');
        s            = s.strip();
        current_char = s.pop_front();
        if (current_char == '\"')
            value = parse_string(s);
        else if (current_char == 't' || current_char == 'f')
            value = parse_bool(s);
        else if (current_char == 'n')
            value = parse_null(s);
        else if (current_char == '-' || isdigit(current_char))
            value = parse_number(s);
        else if (current_char == '[')
            value = parse_array(s);
        else if (current_char == '{')
            value = parse_object(s);
        else
            assert(0);
        auto pair_obj = pair<string, shared_ptr<JsonBase>>(key, value);
        res->value.push_back(std::move(pair_obj));

        while (isspace(current_char) && s.len() > 0) {
            current_char = s.pop_front();
        }
        if (current_char == '}') {
            current_char = s.pop_front();
            return shared_ptr<JsonBase>(res);
        }
        assert(current_char == ',');
        s            = s.strip();
        current_char = s.pop_front();
    }
    assert(0);
    return shared_ptr<JsonBase>(res);
}

shared_ptr<JsonBase> JsonParser::parse_array(CCharArray &s) {
    JsonArray *res = new JsonArray;
    assert(current_char == '[');
    s            = s.strip();
    current_char = s.pop_front();
    while (s.len() > 0) {
        if (current_char == '\"')
            res->value.push_back(parse_string(s));
        else if (current_char == 't' || current_char == 'f')
            res->value.push_back(parse_bool(s));
        else if (current_char == 'n')
            res->value.push_back(parse_null(s));
        else if (current_char == '-' || isdigit(current_char))
            res->value.push_back(parse_number(s));
        else if (current_char == '[')
            res->value.push_back(parse_array(s));
        else if (current_char == '{')
            res->value.push_back(parse_object(s));
        else if (current_char == ']') {
            current_char = s.pop_front();
            return shared_ptr<JsonBase>(res);
        } else
            assert(0);

        while (isspace(current_char) && s.len() > 0) {
            current_char = s.pop_front();
        }
        if (current_char == ']') {
            current_char = s.pop_front();
            return shared_ptr<JsonBase>(res);
        }
        assert(current_char == ',');
        s            = s.strip();
        current_char = s.pop_front();
    }
    assert(0);
    return shared_ptr<JsonBase>(res);
}

shared_ptr<JsonBase> JsonParser::parse_number(CCharArray &s) {
    ostringstream os;
    JsonNumber *  res = new JsonNumber;
    assert(current_char == '-' || isdigit(current_char));
    set<char> special = {'+', '-', '.', 'e'};
    // we do not check whether the number is illegal here
    while (current_char != EOF &&
           (isdigit(current_char) ||
            (special.find(current_char) != special.end()))) {
        os << current_char;
        current_char = s.pop_front();
    }
    istringstream in(os.str());
    in >> res->value;
    return shared_ptr<JsonBase>(res);
}
shared_ptr<JsonBase> JsonParser::parse_bool(CCharArray &s) {
    ostringstream os;
    JsonBoolean * res = new JsonBoolean;
    os << current_char;
    if (current_char == 't') {
        for (uint16_t i = 0; i < 3 && s.len() > 0; i++) {
            current_char = s.pop_front();
            os << current_char;
        }
        assert(os.str() == "true");
        res->value   = true;
        current_char = s.pop_front();
        return shared_ptr<JsonBase>(res);
    } else if (current_char == 'f') {
        for (uint16_t i = 0; i < 4 && s.len() > 0; i++) {
            current_char = s.pop_front();
            os << current_char;
        }
        assert(os.str() == "false");
        res->value   = false;
        current_char = s.pop_front();
        return shared_ptr<JsonBase>(res);
    } else {
        assert(0);
        return shared_ptr<JsonBase>(res);
    }
}
shared_ptr<JsonBase> JsonParser::parse_null(CCharArray &s) {
    ostringstream os;
    assert(current_char == 'n');
    os << current_char;
    for (uint16_t i = 0; i < 3; i++) {
        current_char = s.pop_front();
        os << current_char;
    }
    JsonNull *res = new JsonNull;
    res->value    = os.str();
    assert(res->value == "null");
    current_char = s.pop_front();
    return shared_ptr<JsonBase>(res);
}
shared_ptr<JsonBase> JsonParser::parse_string(CCharArray &s) {
    ostringstream os;
    JsonString *  res = new JsonString;
    assert(current_char == '\"');
    while (s.len() > 0) {
        current_char = s.pop_front();
        if (current_char == '\"') {
            res->value   = os.str();
            current_char = s.pop_front();
            return shared_ptr<JsonBase>(res);
        }
        os << current_char;
    }
    // error
    assert(0);
    return shared_ptr<JsonBase>(res);
}
};  // namespace BmcvJson