#include<jotjson/parse.hpp>

#include<cstdint>
#include<memory>
#include<stdexcept>
#include<string>
#include<utility>

namespace jotjson{
    namespace {
        bool is_digit(char c){
            return c >= '0' && c <= '9';
        }

        int hex_value(char c){
            if(c >= '0' && c <= '9'){
                return c - '0';
            }else if(c >= 'a' && c <= 'f'){
                return 10 + (c - 'a');
            }else if(c >= 'A' && c <= 'F'){
                return 10 + (c - 'A');
            }else{
                return -1;
            }
        }

// 解码一个 UTF-8 码点。
//
// JSON 要求字符串是 UTF-8。
// 这个函数检查：
// - 首字节是否合法；
// - 后续字节是否以 10 开头；
// - 是否使用了过长编码；
// - 是否超过 Unicode 最大码点；
// - 是否落在 UTF-16 代理项区间。
        bool decode_utf8(const std::string& text,std::size_t offset,std::size_t& length,std::uint32_t& code_point){
            if(offset >= text.size()){
                return false;
            }
            const unsigned char first = static_cast<unsigned char>(text[offset]);

            // ascii直接返回
            if(first <= 0x7f){
                length = 1;
                code_point = first;
                return true;
            }

            std::size_t sequence_length = 0;
            std::uint32_t minimum = 0;
            std::uint32_t value = 0;

            if(first >= 0xC2 && first <= 0xDF){
                // 两字节
                sequence_length = 2;
                minimum = 0x80;
                value = first & 0x1F;
            }else if(first >= 0xE0 && first <= 0xEF){
                // 三字节
                sequence_length = 3;
                minimum = 0x800;
                value = first & 0x0F;

            }else if(first >= 0xF0 && first <= 0xF4){
                sequence_length = 4;
                minimum = 0x10000;
                value = first & 0x07;
            }else{
                return false;
            }

            // 检查后续字节
            if(sequence_length > text.size() - offset){
                return false;
            }

            for(std::size_t i = 1;i < sequence_length;i++){
                const unsigned char next = static_cast<unsigned char>(text[offset + i]);

                //后续必须是10xxx
                if((next & 0xC0) != 0x80){
                    return false;
                }
                value = (value << 6) | (next & 0x3f);
            }
             // 检查过长编码和 Unicode 范围。
            if (value < minimum || value > 0x10FFFF) {
                return false;
            }

            // 代理项不能单独出现在 UTF-8 中。
            if (value >= 0xD800 && value <= 0xDFFF) {
                return false;
            }

            length = sequence_length;
            code_point = value;
            return true;
        }
        // 将一个 Unicode 码点编码为 UTF-8 并追加到字符串。
        void append_utf8(
            std::string& output,
            std::uint32_t code_point) {
            if (code_point <= 0x7F) {
                output.push_back(static_cast<char>(code_point));
            } else if (code_point <= 0x7FF) {
                output.push_back(
                    static_cast<char>(0xC0 | (code_point >> 6)));
                output.push_back(
                    static_cast<char>(0x80 | (code_point & 0x3F)));
            } else if (code_point <= 0xFFFF) {
                output.push_back(
                    static_cast<char>(0xE0 | (code_point >> 12)));
                output.push_back(
                    static_cast<char>(
                        0x80 | ((code_point >> 6) & 0x3F)));
                output.push_back(
                    static_cast<char>(0x80 | (code_point & 0x3F)));
            } else {
                output.push_back(
                    static_cast<char>(0xF0 | (code_point >> 18)));
                output.push_back(
                    static_cast<char>(
                        0x80 | ((code_point >> 12) & 0x3F)));
                output.push_back(
                    static_cast<char>(
                        0x80 | ((code_point >> 6) & 0x3F)));
                output.push_back(
                    static_cast<char>(0x80 | (code_point & 0x3F)));
            }
        }
    }

// 内部解析器。
//
// 它保存输入副本和当前游标位置。
// 每次解析一个值，就构造一个 value 节点。
class parser{

public:
    parser(const char* data,std::size_t size,std::size_t max_depth)
    :input_(data == nullptr ? "":data,size)
    ,cursor_(0),line_(1),max_depth_(max_depth),column_(1){}

    document parse_document(){
         // JSON 顶层允许任意类型，因此这里不要求一定是对象。
         skip_space();

         if(at_end()){
            fail(error_code::parse,"empty json input");
         }

         std::unique_ptr<value> root = parse_value(0);
         skip_space();

         if(!at_end()){
            fail(error_code::parse,"unexpected trailing characters");
         }

         return document(std::move(root));
    }

private:
    std::string input_;
    std::size_t cursor_;
    std::size_t line_;
    std::size_t max_depth_;
    std::size_t column_;

    bool at_end() const{ return cursor_ >= input_.size();}

    char peek() const{ return input_[cursor_];}

    error_location location() const{
        return error_location(cursor_,line_,column_);
    }
    // 在当前位置抛出解析异常。
    void fail(error_code code,const std::string& message)const{
        throw parse_error(code,location(),message);
    }

    // 在指定位置抛出解析异常。
    // 用于重复键等需要保存起始位置的场景。
    void fail_at(error_code code,error_location where,const std::string& message) const{
        throw parse_error(code,where,message);
    }

    // 消费一个字符并更新行列号。
    void advance(){
        const char consumed = input_[cursor_];
        ++cursor_;

        // 
        if(consumed == '\n'){
            ++line_;
            column_ = 1;
        }else{
            ++ column_;
        }
    }

    // 期望字符
    bool consume(char expected){
        if(!at_end() && peek() == expected){
            advance();
            return true;
        }
        return false;
    }

    // 当前字符必须是指定字符，否则报错。
    void expect(char expected,const std::string& message){
        if(at_end() || peek() != expected){
            fail(error_code::parse,message);
        }
        advance();
    }

    // 跳过 JSON 允许的四种空白字符。
    //
    // JSON 不允许把其他 Unicode 空白直接当作结构空白。
    void skip_space(){
        while(!at_end()){
            const char c = peek();
            if(c == ' ' || c == '\t' || c == '\n' || c == '\r'){
                advance();
            }else{
                break;
            }
        }
    }

     // 解析一个值。
    //
    // depth 表示当前嵌套层数。
    std::unique_ptr<value> parse_value(std::size_t depth){
        if(depth > max_depth_){
            fail(error_code::depth_limit,"exceed maximum depth limit");
        }
        skip_space();
        if(at_end()){
            fail(error_code::parse,"unexpected end of input");
        }

        switch(peek()){
            case 'n':
                match_literal("null");
                return make_null();
            case 't':
                return parse_bool(true);
            case 'f':
                return parse_bool(false);
            case '"':
                return make_string(parse_string_content());
            case '[':
                return parse_array(depth);
            case '{':
                return parse_object(depth);
            default:
                if(peek() == '-' || is_digit(peek())){
                    return parse_number(parse_number_lexeme());
                }else{
                    fail(error_code::parse,"unexpected character");
                }
        }

        // 这里的 return 只是为了满足编译器的路径分析。
        // fail() 实际上总会抛出异常。
        return std::unique_ptr<value>();

    }

    // 匹配 null、true、false 等固定文本。
    void match_literal(const char* literal){
        const char* current = literal;
        while(*current != '\0'){
            if(at_end() || peek() != *current){
                fail(error_code::parse,"unexpected character");
            }
            advance();
            ++current;
        }
    }

    std::unique_ptr<value> make_null(){
        return std::unique_ptr<value>(new value(value_type::nil));
    }

    std::unique_ptr<value> make_bool(bool input){
        std::unique_ptr<value> result(new value(value_type::boolean));
        result->set_bool(input);
        return result;
    }

    std::unique_ptr<value> make_string(std::string input){
        std::unique_ptr<value> result(new value(value_type::string));
        result->set_string(std::move(input));
        return result;
    }
    
    std::unique_ptr<value> make_number(std::string input){
        std::unique_ptr<value> result(new value(value_type::number));
        result->set_number(std::move(input));
        return result;
    }

    // 严格扫描 JSON 数字语法。
    //
    // 数字暂时只保存原始文本。
    // 真正转换为 int64_t 或 double 发生在访问函数中。
    std::string parse_number_lexeme(){
        std::size_t start = cursor_;
        if(consume('-')){
            // 负号
            if(at_end()){
                fail(error_code::parse,"invalid number");
            }
        }


        // 整数部分。
        // 注意：consume('0') 已经消费了字符，
        // 这里不能再调用 advance()，否则会跳过下一个字符。
        if(peek() == '0'){
            advance();

            if(!at_end() && is_digit(peek())){
                fail(error_code::parse,"invalid number with leading zero");
            }
        }else if(peek() >= '1' && peek() <= '9'){
            do{
                advance();
            }while(!at_end() && is_digit(peek()));
        }else{
            fail(error_code::parse,"invalid number");
        }

          // 可选小数部分。
          if(!at_end() && peek() =='.'){
            advance();

            if(at_end() || !is_digit(peek())){
                fail(error_code::parse,"fraction part is missing");
            }
            do{
                advance();
            }while(!at_end() && is_digit(peek()));
          }
          // 可选指数部分。
          if(!at_end() && (peek() =='e' || peek() =='E')){
            advance();

            if(!at_end() && (peek() =='+' || peek() =='-')){
                advance();
            }

            if(at_end() || !is_digit(peek())){
                fail(error_code::parse,"exponent part is missing");
            }

            do{
                advance();
            }while(!at_end() && is_digit(peek()));
          }
        return input_.substr(start,cursor_ - start);
    }

     // 读取 \u 后面的四位十六进制数字。
     std::uint32_t read_hex4(){
        std::uint32_t result = 0;

        for(int i = 0;i < 4;i++){
            if(at_end()){
                fail(error_code::parse,"unexpected end of input in unicode escape");
            }

            const int digit = hex_value(peek());
            if(digit < 0){
                fail(error_code::parse,"invalid hex digit in unicode escape");
            }

            result = (result << 4) | static_cast<std::uint32_t>(digit);

            advance();
        }
        return result;
     }

     // 读取字符串内容，不包含最外层引号。
     std::string parse_string_content(){
        if(at_end() || peek() != '"'){
            fail(error_code::parse,"expected '\"' at the beginning of string");
        }
        advance(); // consume the opening quote

        std::string result;
        while(!at_end()){
            const unsigned char current = static_cast<unsigned char>(peek());

            if(current == '"'){
                advance(); // consume the closing quote
                return result;
            }

            if(current < 0x20){
                fail(error_code::parse,"unescaped control character in string");
            }

            if(current =='\\'){

                advance();
                if(at_end()){
                    fail(error_code::parse,"unexpected end of input in string escape");
                }

                const char escaped = peek();
                advance();
                switch(escaped){
                    case '"':
                        result.push_back('"');
                        break;
                    case '\\':
                        result.push_back('\\');
                        break;
                    case '/':
                        result.push_back('/');
                        break;
                    case 'b':
                        result.push_back('\b');
                        break;
                    case 'f':
                        result.push_back('\f');
                        break;
                    case 'n':
                        result.push_back('\n');
                        break;
                    case 'r':
                        result.push_back('\r');
                        break;
                    case 't':
                        result.push_back('\t');
                        break;
                    case 'u':{

                        std::uint32_t code_point = read_hex4();
                        // 高代理项必须跟一个低代理项。
                        if (code_point >= 0xD800 &&
                            code_point <= 0xDBFF) {
                            if (input_.size() - cursor_ < 6 ||
                                input_[cursor_] != '\\' ||
                                input_[cursor_ + 1] != 'u') {
                                fail(
                                    error_code::parse,
                                    "high surrogate is missing low surrogate");
                            }

                            advance(); // '\'
                            advance(); // 'u'

                            const std::uint32_t low =
                                read_hex4();

                            if (low < 0xDC00 ||
                                low > 0xDFFF) {
                                fail(
                                    error_code::parse,
                                    "invalid low surrogate");
                            }

                            code_point =
                                0x10000 +
                                ((code_point - 0xD800) << 10) +
                                (low - 0xDC00);
                        } else if (code_point >= 0xDC00 &&
                                code_point <= 0xDFFF) {
                            fail(
                                error_code::parse,
                                "unexpected low surrogate");
                        }

                        append_utf8(result, code_point);
                        break;
                    }

                    default:
                        fail(error_code::parse,"invalid escape character");
                }
            }else if(current < 0x80){
                // ASCII 快速路径，不需要 UTF-8 解码。
                result.push_back(static_cast<char>(current));
                advance();
            }else{
                // 非 ASCII 字符，需要解码 UTF-8。
                std::size_t length = 0;
                std::uint32_t code_point = 0;

                if(!decode_utf8(input_,cursor_,length,code_point)){
                    fail(error_code::invalid_utf8,"invlid utf-8 sequence");
                }
                result.append(input_,cursor_,length);

                for(std::size_t i = 0;i < length;i++){
                    advance();
                }
            }
        }
        fail(error_code::parse,"unterminated string");

        return std::string();
    }

    std::unique_ptr<value> parse_array(std::size_t depth){
        advance(); // '[
        skip_space();

        std::unique_ptr<value> result(new value(value_type::array));

        // 空数组
        if(consume(']')){
            return result;
        }

        // 非空数组至少有一个元素。
        while(true){
            std::unique_ptr<value> child = parse_value(depth+1);

            result->append_array(std::move(child));
            skip_space();

            if(consume(',')){
                skip_space();
                continue;
            }

            if(consume(']')){
                break;
            }

            fail(error_code::parse,"expected ',' or ']' after array element");
        }

        return result;
    }

    std::unique_ptr<value> parse_object(std::size_t depth){
        advance(); // {
        skip_space();

        std::unique_ptr<value> result(new value(value_type::object));

        // 空对象
        if(consume('}')){
            return result;
        }

        while(true){
            // 对象键必须是字符串
            if(at_end() || peek() !='"'){
                fail(error_code::parse,"object key must be a string");
            }

            // 记录键的起始位置，
            // 重复键报错时指向这里，而不是值。
            const error_location key_location = location();

            std::string key = parse_string_content();

              // 检查重复键。
            // 因为 key 已经解码，
            // "a" 和 "\u0061" 也会被识别为重复。
            if(result->find(key) != nullptr){
                fail_at(error_code::parse,key_location,"duplicate object key");
            }

            skip_space();
            expect(':',"expected ':' after object key");

            std::unique_ptr<value> child = parse_value(depth+1);
            result->append_object(std::move(key),std::move(child));

            skip_space();

            if(consume(',')){
                skip_space();
                continue;
            }

            if(consume('}')){
                break;
            }
            fail(error_code::parse,"expected ',' or '}' after object member");
        }

        return result;
    }

    std::unique_ptr<value> parse_bool(bool input){
        if(input){
            match_literal("true");
        }else{
            match_literal("false");
        }
        return make_bool(input);
    }

    std::unique_ptr<value> parse_number(std::string lexeme){
        return make_number(std::move(lexeme));
    }

};


document parse_copy(const char* data,std::size_t size,const parse_options& options){
    if(size > 0 && data== nullptr){
        throw std::invalid_argument("input data is null but size is greater than zero");
    }

    if(options.max_depth == 0){
        throw std::invalid_argument("max_depth must be greater than zero");
    }

    parser instance(data,size,options.max_depth);

    return instance.parse_document();
}

document parse_copy(const std::string& data,const parse_options& options){
    return parse_copy(data.data(),data.size(),options);
}
}
