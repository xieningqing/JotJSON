#include<jotjson/value.hpp>

#include<cerrno>
#include<stdexcept>
#include<utility>

namespace jotjson{

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

        // strtoll 出错时通过 errno 返回 ERANGE。
        // 每次调用前清零，避免受上一次转换影响。
        errno = 0;

        char* end = nullptr;
        const long long parsed = 
        std::strtoll(text.c_str(),&end,10);

        // 没有解析完
        if(errno == ERANGE || 
        end != text.c_str() + text.size()){
            throw std::out_of_range(
                "number can not represented as int64"
            );
        }

        return static_cast<std::int64_t>(parsed);
    }

    double value::as_double() const{
        const std::string& text = number_lexeme();
        errno = 0;
        char* end = nullptr;
        const double parsed = 
        std::strtod(
            text.c_str(),&end);
        if(errno == ERANGE || end != text.c_str() + text.size()){
            throw std::out_of_range(
                "number can not represented as double"
            );
        }
        return parsed;
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
