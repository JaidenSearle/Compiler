#pragma once

//#include "main.cpp"

enum class tokenType{
    int_lit, 
    l_paren,
    r_paren,
    _return,
    int_add,
    sub,
    multiply,
    div,
    l_c_bracket,
    r_c_bracket,
    l_s_bracket,
    r_s_bracket,
    less_than,
    greater_than,
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
    inline tokenizer(std::string& str) : m_str(std::move(str)) {


}
inline std::vector<token> tokenize(){
    std::vector<token> tokens;
  size_t i = 0;
  const size_t n = m_str.size();

  while (i < n) {
      char c = m_str[i];

      // Identifier / keyword
      if (std::isalpha(static_cast<unsigned char>(c))) {
          std::string buf;

          while (i < n && std::isalnum(static_cast<unsigned char>(m_str[i]))) {
              buf.push_back(m_str[i]);
              ++i;
            }

          if (buf == "return") {
              tokens.push_back({ tokenType::_return, std::nullopt });
            
            }
          else {
              std::cerr << "error: unexpected identifier '" << buf << "'\n";
              std::exit(EXIT_FAILURE);
            }
        }
      // Integer literal
      else if (std::isdigit(static_cast<unsigned char>(c))) {
          std::string buf;

          while (i < n && std::isdigit(static_cast<unsigned char>(m_str[i]))) {
              buf.push_back(m_str[i]);
              ++i;
            }

          tokens.push_back({ tokenType::int_lit, buf });
         
        }
      
      // Whitespace (skip)
        else if (std::isspace(static_cast<unsigned char>(c))) {
          i++;
        }
      //character tokens
        else if (c == '+') {
        tokens.push_back({tokenType::int_add});
            i++;
        }
        else if (c == '('){
            tokens.push_back({tokenType::l_paren});
            i++;

        }
        else if (c == ')'){
            tokens.push_back({tokenType::r_paren});
            i++;

        }
        else if (c == '['){
            tokens.push_back({tokenType::l_s_bracket});
            i++;

        }
      else if (c == ']'){
            tokens.push_back({tokenType::r_s_bracket});
            i++;

        }
      else if (c == '{'){
            tokens.push_back({tokenType::l_c_bracket});
            i++;

        }
      else if (c == '}'){
            tokens.push_back({tokenType::r_c_bracket});
            i++;

        }
      else if (c == '<'){
            tokens.push_back({tokenType::less_than});
            i++;

        }
      else if (c == '>'){
            tokens.push_back({tokenType::greater_than});
            i++;

        }
      else if (c == '+'){
            tokens.push_back({tokenType::int_add});
            i++;

        }
      else if (c == '-'){
            tokens.push_back({tokenType::sub});
            i++;

        }
      else if (c == '*'){
            tokens.push_back({tokenType::multiply});
            i++;

        }
      else if (c == '/'){
            tokens.push_back({tokenType::div});
            i++;

        }
      else if (c == ':'){
            tokens.push_back({tokenType::colon});
            i++;

        }

      else {
          std::cerr << "error: unknown character '" << c << "' at position " << i << "\n";
          std::exit(EXIT_FAILURE);
        }
    }

  return tokens;

}

private:

[[nodiscard]] std::optional<char> peak(int x = 1) const{
    
    if(m_index + x >= m_str.length()){

        return {};
    }

    else{
        
       return m_str.at(m_index); 
    }
}

char consume() {

    return m_str.at(m_index++);
}

const std::string m_str;
int m_index;

};