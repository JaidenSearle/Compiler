#pragma once

#include <string>
#include <vector>
#include <optional>
#include <unordered_set>
#include <iostream>
#include <cctype>

enum class tokenType{
   identifier,
   keyword,
   punctuator,
   op,
   literal,
   constant

};

struct token{
tokenType type;
std::optional<std::string> value;

};

class tokenizer{

public:

    inline tokenizer(const std::string& str) : m_str(str) {}


    inline std::vector<token> tokenize(){
        std::vector<token> tokens;
        size_t i = m_index;
        const size_t n = m_str.size();

        while (i < n) {
            char c = m_str[i];

            if(std::isspace(static_cast<unsigned char>(c))){
                i++;
                continue;
            }

        
            if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            std::string buf;

                while (i < n && (std::isalnum(static_cast<unsigned char>(m_str[i])) || m_str[i] == '_')){
                    buf.push_back(m_str[i]);
                    i++;
                }

                static const std::unordered_set<std::string> keywords = {
                    "int", "str", "char", "double", "float", "arr"

                };

                //keywords
                if(keywords.count(buf)){
                    tokens.push_back({tokenType::keyword, buf});

                }
                
                //identifiers
                else{
                    tokens.push_back({tokenType::identifier, buf});

                }
                continue;
            }

            //constant
            if(std::isdigit(static_cast<unsigned char>(c))){
                std::string num;

                while(i < n && std::isdigit(static_cast<unsigned char>(m_str[i]))){
                    num.push_back(m_str[i]);
                    i++;
                }
                tokens.push_back({tokenType::constant, num});
                continue;
            }

            //operators
            if(std::string("+-*/%=<>").find(c) != std::string::npos){
                tokens.push_back({tokenType::op, std::string(1, c)}); 
                i++;
                continue;
            }

            //punctuators
            if(std::string("\"'_(){}[]:;").find(c) != std::string::npos){
                tokens.push_back({tokenType::punctuator, std::string(1, c)});
                i++;
                continue;


            }

            //literals
            if (m_str[i] == '"'){
                std::string str_lit;
                str_lit.push_back(m_str[i]); 
                i++;

                while (i < n){
                    char c = m_str[i];

                    if (c == '\\'){
                        
                        if (i + 1 < n){
                            str_lit.push_back(c);
                            str_lit.push_back(m_str[i + 1]);
                            i += 2;
                        } else {
                            std::cerr << "error: unfinished escape sequence in string literal\n";
                            std::exit(EXIT_FAILURE);
                        }

                    } 
                    else if (c == '"'){
                        str_lit.push_back(c); 
                        i++;
                        break;

                    } 
                    else{
                        str_lit.push_back(c);
                        i++;
                    }
                }
                tokens.push_back({tokenType::literal, str_lit});
                continue;

            }

        else{
                std::cerr << "error: unknown character '" << c << "' at position " << i << "\n";
                std::exit(EXIT_FAILURE);
            }
            
        }
        return tokens;

 
    }
private:

const std::string m_str;
int m_index = 0;

};