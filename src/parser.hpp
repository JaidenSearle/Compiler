#pragma once

#include <vector>
#include <string>
#include <iostream>
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
        
        if(m_tokens[i].type == tokenType::type_keyword)
        {
            ASTNode *decl = parse_declaration(i);
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
        else
        {
            ASTNode *expr = parse_expr(i);
            if(expr)
            {
                root -> children.push_back(expr);
            }
            else
            {
                std::cout << " >>> error while parsing" << std::endl;
                i++;
            }
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
            //build node if variable is initilized with a constant
           
            ASTNode *valNode = MakeNode(NodeType::Constant, m_tokens[i].value);
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

 //TODO add recursive descent parsing for expressions
   
    //non-terminal functions
    ASTNode *parse_expr(size_t& i)
    {
       if(i >= m_tokens.size()) return nullptr;

       ASTNode *left = parse_term(i);
       if(!left) return nullptr;

       while(i < m_tokens.size() && (m_tokens[i].value == "+" || m_tokens[i].value == "-"))
       {
           std::string op = m_tokens[i].value;
           i++;
           ASTNode *right = parse_term(i);
           if(!right)
           {
               std::cout << " >>> expected expression after operator" << std::endl;
               return nullptr;
           }
           ASTNode *opNode = MakeNode(NodeType::BinOp, op);
           opNode->children.push_back(left);
           opNode->children.push_back(right);
           left = opNode;
       }

        return left;
    }
    
    ASTNode *parse_term(size_t& i)
    {
        if(i >= m_tokens.size()) return nullptr;

        ASTNode *left = parse_factor(i);
        if(!left) return nullptr;

        auto startsFactor = [&](size_t idx)->bool 
        {
            if(idx >= m_tokens.size()) return false;
            if(m_tokens[idx].type == tokenType::lparen) return true;
            if(m_tokens[idx].type == tokenType::constant) return true;
            if(m_tokens[idx].type == tokenType::identifier) return true;
            if(m_tokens[idx].type == tokenType::literal) return true;
            return false;
        };

        while(i < m_tokens.size())
        {
            std::string op;
            if(m_tokens[i].value == "*" || m_tokens[i].value == "/")
            {
                op = m_tokens[i].value;
                i++; 
            }
            else if(startsFactor(i))
            {
              
                op = "*";
            }
            else
            {
                break;
            }

            ASTNode *right = parse_factor(i);
            if(!right)
            {
                std::cout << " >>> expected expression after operator" << std::endl;
                return nullptr;
            }
            ASTNode *opNode = MakeNode(NodeType::BinOp, op);
            opNode->children.push_back(left);
            opNode->children.push_back(right);
            left = opNode;
        }

        return left;
    }
   
    ASTNode *parse_factor(size_t& i)
    {
        if(i >= m_tokens.size()) return nullptr;

        if(m_tokens[i].type == tokenType::constant) 
        {
            ASTNode *valNode = MakeNode(NodeType::Constant, m_tokens[i].value);
            i++;
            return valNode;
        }
         if(m_tokens[i].type == tokenType::identifier)
        {
            ASTNode *valNode = MakeNode(NodeType::Identifier, m_tokens[i].value);
            i++;
            return valNode;
        }

        if(m_tokens[i].type == tokenType::lparen)
        {
            i++; 
            ASTNode *node = parse_expr(i);
            
            if(!node) return nullptr;
            if(i >= m_tokens.size() || m_tokens[i].type != tokenType::rparen)
            {
                std::cout << " >>> expected ')'" << std::endl;
                return nullptr;
            }
            i++; 
            return node;
        }

        return nullptr;

    }
        
    const std::vector<token> m_tokens;
    size_t m_index = 0;

}; 