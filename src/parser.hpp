#pragma once

#include <vector>
#include <string>
#include "tokenizer.hpp"

enum class NodeType
{
Program, 
Declaration,
Expression, 
Identifier, 
BinOp, 
Constant,
Literal,
punctuator,
newline

};

struct ASTNode
{
NodeType type;
std::string value;
std::vector<ASTNode*> children;


};
ASTNode *MakeNode(NodeType type, const std::string &value = "")
{
return new ASTNode{type, value, {}};

};


class parser{
    
public:
inline explicit parser(std::vector<token> tokens) : m_tokens(std::move(tokens)) {}

ASTNode *parse_program()

{
ASTNode *root = MakeNode(NodeType::Program);

    size_t i = 0;
    while(i < m_tokens.size())
    {
        if(m_tokens[i].type == tokenType::newline)
        {
            i++;
            continue;
        }

        ASTNode* decl = parse_declaration(i);
        if(decl)
        {
            root -> children.push_back(decl);

        }
        else
        {
            std::cout << " >>> error while parsing" << std::endl;
            i++;
        }
         
    }
    return root;
}

private:

    ASTNode *parse_declaration(size_t& i)
    { 
        if(i >= m_tokens.size() || m_tokens[i].type != tokenType::type_keyword)
        {
            std::cout << " >>> expected a type identifier before variable declaration" << std::endl;
            return nullptr;
        }
        ASTNode *declNode = MakeNode(NodeType::Declaration, m_tokens[i].value);
        i++;

        
            if(i >= m_tokens.size() || m_tokens[i].type != tokenType::identifier)
            {
                std::cout << " >>> expected an identifier" << std::endl;
                return nullptr;
            }
            //build node if identifier is found
            ASTNode *identNode = MakeNode(NodeType::Identifier, m_tokens[i].value);
            i++;
        
            if(i >= m_tokens.size() || m_tokens[i].type != tokenType::op)
            {
                std::cout <<" >>> expected an '=' after variable declaration" << std::endl;
                return nullptr;
            }
            //build node if operator is found
            ASTNode *opNode = MakeNode(NodeType::BinOp, m_tokens[i].value);
            i++;
            
            if(i >= m_tokens.size() || m_tokens[i].type != tokenType::identifier && m_tokens[i].type != tokenType::literal && m_tokens[i].type != tokenType::constant)
            {
                std::cout << " >>> variable declarations must be initilized with a value" << std::endl;
                return nullptr;
            }
            //build node if variable is initilized with a literal
           
            ASTNode *valNode = MakeNode(NodeType::Literal, m_tokens[i].value);
            i++;
            
        //build tree
            opNode -> children.push_back(identNode);
            opNode -> children.push_back(valNode);
            declNode -> children.push_back(opNode);

            if( i >= m_tokens.size() || m_tokens[i].type != tokenType::newline)
            {

                std::cout << " >>> cannot have declarations on the same line" <<  std::endl;
                return nullptr;
                
            }
            
        else if(m_tokens[i].value == "arr")
        {
            i++;
            //TODO: add array handling

        }

        std::cout << " >>> parse sucessful" << std::endl;
        return declNode;
    }


    ASTNode *parse_expression(size_t& i)
    {
        //TODO add recursive descent parsing for expressions




        
    }
    const std::vector<token> m_tokens;
    size_t m_index = 0;

}; 