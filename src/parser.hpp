#pragma once

#include <vector>
#include <string>
#include "tokenizer.hpp"



class parser{
    
public:
inline explicit parser(std::vector<token> tokens) : m_tokens(std::move(tokens)) {}

void parse_program()
{
    size_t i = 0;
    while(i < m_tokens.size())
    {
        if(m_tokens[i].type == tokenType::newline)
        {
            i++;
            continue;
        }
        else if(!parse_declaration(i))
        {
            std::cout << " >>> bad token" << std::endl;
            i++;
        }
        
            
        
    }

}

private:

bool parse_declaration(size_t& i){
        
    if(i >= m_tokens.size() || m_tokens[i].type != tokenType::type_keyword)
    {
        std::cout << " >>> expected a type identifier before variable declaration" << std::endl;
        return false;
    }
    i++;

    if(i >= m_tokens.size() || m_tokens[i].type != tokenType::identifier)
    {
        std::cout << " >>> expected an identifier" << std::endl;
        return false;
    }
    i++;

     if(i >= m_tokens.size() || m_tokens[i].type != tokenType::op)
     {
        std::cout <<" >>> expected an '=' after variable declaration" << std::endl;
        return false;
     }
     i++;

     if(i >= m_tokens.size() || m_tokens[i].type != tokenType::identifier && m_tokens[i].type != tokenType::literal && m_tokens[i].type != tokenType::constant)
     {
        std::cout << " >>> variable declarations must be initilized with a value" << std::endl;
        return false;

     }
     i++;

     if( i >= m_tokens.size() || m_tokens[i].type != tokenType::newline)
     {

        std::cout << " >>> cannot have declarations on the same line" << std::endl;
        
     }
     i++;

     std::cout << " >>> parse sucessful" << std::endl;
     return true;

    }

    const std::vector<token> m_tokens;
    size_t m_index = 0;

};