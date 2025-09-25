#pragma once

#include <vector>
#include <string>
#include "tokenizer.hpp"

enum class NodeType
{
Program, 
Declaration, 
Identifier, 
BinOp, 
Literal

};

struct ASTNode
{
NodeType type;
std::string value;
std::vector<ASTNode*> children;


};
ASTNode* MakeNode(NodeType type, const std::string &value = "")
{
return new ASTNode{type, value, {}};

};


class parser{
    
public:
inline explicit parser(std::vector<token> tokens) : m_tokens(std::move(tokens)) {}

ASTNode* parse_program()
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
            std::cout << " >>> error while parsing" << std::endl;
            i++;
        }
         
    }

}

private:

ASTNode* parse_declaration(size_t& i)
{ 
    if(i >= m_tokens.size() || m_tokens[i].type != tokenType::type_keyword)
    {
        std::cout << " >>> expected a type identifier before variable declaration" << std::endl;
        return nullptr;
    }

    else if(m_tokens[i].value == "int" || m_tokens[i].value == "str")
    {
        i++;
        //build type identifier node
        if(i >= m_tokens.size() || m_tokens[i].type != tokenType::identifier)
        {
            std::cout << " >>> expected an identifier" << std::endl;
            return nullptr;
        }
        i++;
        //build node if identifier is found
        if(i >= m_tokens.size() || m_tokens[i].type != tokenType::op)
        {
            std::cout <<" >>> expected an '=' after variable declaration" << std::endl;
            return nullptr;
        }
        i++;
        //build node if operator is found
        if(i >= m_tokens.size() || m_tokens[i].type != tokenType::identifier && m_tokens[i].type != tokenType::literal && m_tokens[i].type != tokenType::constant)
        {
            std::cout << " >>> variable declarations must be initilized with a value" << std::endl;
            return nullptr;
        }
        i++;
        //build node if variable is initilized
        if( i >= m_tokens.size() || m_tokens[i].type != tokenType::newline)
        {

            std::cout << " >>> cannot have declarations on the same line" <<  std::endl;
            return nullptr;
            
        }
        i++;
    }

    else if (m_tokens[i].value == "double" || m_tokens[i].value == "float")
    {
        i++;

        if(i >= m_tokens.size() || m_tokens[i].type != tokenType::identifier)
        {
            std::cout << " >>> expected an identifier" << std::endl;
            return nullptr;
        }
        i++;

        if(i >= m_tokens.size() || m_tokens[i].type != tokenType::op)
        {
            std::cout <<" >>> expected an '=' after variable declaration" << std::endl;
            return nullptr;
        }
        i++;
        if(i >= m_tokens.size() || m_tokens[i].type != tokenType::constant)
        {
            std::cout << "expected a value" << std::endl;
            return nullptr;
        }
        i++;
        if(i >= m_tokens.size() || m_tokens[i].type != tokenType::punctuator)
        {
            std::cout << "value mismatch floats and doubles must not be a whole number" << std::endl;
            return nullptr;

        }
        i++;
        if(i >= m_tokens.size() || m_tokens[i].type != tokenType::constant)
        {
            std::cout << "expected a value after the decimal" << std::endl;
            return nullptr;

        }
        i++;

    }

    else if(m_tokens[i].value == "arr")
    {
        i++;
        //TODO: add array handling

    }

     std::cout << " >>> parse sucessful" << std::endl;

    }
    ASTNode* parse_expression(size_t& i)
    {


    }
    const std::vector<token> m_tokens;
    size_t m_index = 0;

};