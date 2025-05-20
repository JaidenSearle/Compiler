#pragma once
#include <optional>
#include <string>
#include <vector>
#include "parser.hpp"

enum class tokenType{
    int_lit, 
    l_paren,
    r_paren,
    _return,
    add,
    sub,
    multi,
    div,
    l_c_bracket,
    r_c_bracket,
    l_s_bracket,
    r_s_bracket,
    lt,
    gt,
    equal,
    colon,
    var,

};

struct token{
tokenType type;
std::optional<std::string> value;

};

class tokenizer{

public:
inline tokenizer(std::string str) : m_str(std::move(str)) {


}
inline std::vector<token> tokenize(){
std::string buf;



}


private:
const std::string m_str;


};