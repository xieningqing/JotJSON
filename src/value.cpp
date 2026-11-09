#include<jotjson/value.hpp>

#include<cerrno>
#include<cmath>
#include<stdexcept>
#include<utility>
#include<cstdint>
#include<limits>
#include<locale>
#include<sstream>

namespace jotjson{

    namespace{

        // 整数访问器必须精确处理 INT64_MIN。
        // 这里不用 strtoll 的宽松前缀规则，而是逐位累加并检查边界。
        bool parse_int64_lexeme(const std::string& text,std::int64_t& output){
          if(text.empty()){
            return false;
          }

          std::size_t index = 0;
          const bool negative = text[0] == '-';
          if(negative){
            index++;
          }

          if(index == text.size()){
            return false;
          }

          const std::uint64_t positive_limit =
          static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());

          // 负数多表示一位
          const std::uint64_t limit =
          negative ? positive_limit + static_cast<std::uint64_t>(1) : positive_limit;

          std::uint64_t magnitude = 0;

          for(;index < text.size();index++){
            const char current  = text[index];
            if(current < '0' || current > '9'){
                return false;
            }

            const std::uint64_t digit = static_cast<std::uint64_t>(current - '0');
            // 在 multiplication 之前先判断：
            // magnitude * 10 + digit > limit 是否会发生。
            if(magnitude > (limit - digit) / static_cast<std::uint64_t>(10)){
                return false;
            }
            magnitude = magnitude*static_cast<std::uint64_t>(10) + digit;
          }

          if(negative){
            // 溢出
            if(magnitude == positive_limit + static_cast<std::uint64_t>(1)){
                output = std::numeric_limits<std::int64_t>::min();
            }else{
                output = -static_cast<std::int64_t>(magnitude);
            }
          }else{
            output = static_cast<std::int64_t>(magnitude);
          }
          return true;
        }
     // std::locale::classic() 固定使用 C locale 的规则。
    // 因此进程 locale 即使把小数点设成逗号，JSON 的 1.25 仍按点号解析。
        double parse_double_lexeme(const std::string& text){
            std::istringstream input(text);
            input.imbue(std::locale::classic());

            errno = 0;
            double parsed = 0.0;
            input >> std::noskipws >> parsed;

             // eof 确保整个词法片段都被消费，不能只解析前缀。
            // isfinite 防止把 inf 当成合法数字返回。
            if(!input || !input.eof() || errno == ERANGE || !std::isfinite(parsed)){
                throw std::out_of_range("number can not be represented as double");
            }

            return parsed;
        }
    };

    value::value(value_type type):type_(type),bool_value_(false){}

    value_type value::type() const noexcept{
        return type_;
    }

    bool value::is_null() const noexcept{
        return type_ == value_type::nil;
    }

    bool value::is_bool() const noexcept{
        return type_ == value_type::boolean;
    }

    bool value::is_number() const noexcept{
        return type_ == value_type::number;
    }

    bool value::is_string() const noexcept{
        return type_ == value_type::string;
    }

    bool value::is_array() const noexcept{
        return type_ == value_type::array;
    }
    
    bool value::is_object() const noexcept{
        return type_ == value_type::object;
    }

    bool value::as_bool() const{
        if(!is_bool()){
            throw type_error("value is not boolean");
        }
        return bool_value_;
    }

    const std::string& value::as_string() const{
        if(!is_string()){
            throw type_error("value is not string");
        }
        return string_value_;
    }

    const std::string& value::number_lexeme() const{
        if(!is_number()){
            throw type_error("value is not number");
        }
        return number_lexeme_;
    }

    std::int64_t value::as_int64() const{
        const std::string& text = number_lexeme();
        std::int64_t parsed = 0;

        if(!parse_int64_lexeme(text,parsed)){
            throw std::out_of_range("number can not be represented as int64");
        }
        return parsed;
    }

    double value::as_double() const{
        return parse_double_lexeme(number_lexeme());
    }

    std::size_t value::size() const{
        if(is_array()){
            return array_values_.size();
        }
        if(is_object()){
            return object_keys_.size();
        }
        if(is_string()){
            return string_value_.size();
        }
        return 0;
    }

    const value& value::at(std::size_t index) const{
        if(!is_array()){
            throw type_error("value is not array");
        }
        if(index >= array_values_.size()){
            throw std::out_of_range("array index is out of range");
        }
        return *array_values_[index];
    }

    // object
    const value& value::at(const std::string& key) const{
        if(!is_object()){
            throw type_error("value is not object");
        }
        const value* found = find(key);
        if(found == nullptr){
            throw std::out_of_range("object key does not exist");
        }
        return *found;
    }

    const value* value::find(const std::string& key) const {
        if(!is_object()){
            return nullptr;
        }
         // 阶段 1 使用线性查找。
         // 性能阶段再为大对象增加哈希索引。
        for(std::size_t i = 0;i < object_keys_.size();i++){
            if(object_keys_[i] == key){
                return object_values_[i].get();
            }
        }
        return nullptr;
    }

    bool value::member_at(std::size_t index,std::string* key,const value** out_value)const{
        if(!is_object() || index >= object_keys_.size()){
            return false;
        }
        if(key != nullptr){
            *key = object_keys_[index];
        }

        if(out_value != nullptr){
            *out_value = object_values_[index].get();
        }
        return true;
    }

    void value::set_bool(bool value) {
        bool_value_ = value;
    }
    
    void value::set_string(std::string&& value) {
        // 使用 std::move 转移字符串缓冲区，
        // 避免不必要地再复制一次。
        string_value_ = std::move(value);
    }
    
    void value::set_number(std::string&& lexeme) {
        number_lexeme_ = std::move(lexeme);
    }

    void value::append_array(std::unique_ptr<value> child){
        array_values_.push_back(std::move(child));
    }

    void value::append_object(std::string&& key,std::unique_ptr<value> child){
        object_keys_.push_back(std::move(key));
        object_values_.push_back(std::move(child));
    }

    document::document(std::unique_ptr<value> root):root_(std::move(root)){}

    document::document(document&& other) noexcept :root_(std::move(other.root_)){

    }

    document& document::operator=(document&& other) noexcept{
        if(this != &other){
            root_ = std::move(other.root_);
        }
        return *this;
    }

    document::~document(){
         // root_ 是 unique_ptr。
    // 它析构时会递归析构数组和对象中的子节点，
    // 因此这里不需要手写 delete。
    }
    
    const value& document::root() const {
        if(root_ == nullptr){
            throw std::logic_error("document has no root value");
        }
        return *root_;
    }
}
