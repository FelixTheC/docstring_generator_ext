#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <sstream>
#include <vector>

#include <pybind11/pybind11.h>
#include <pybind11/embed.h>

namespace py = pybind11;

const std::string FILE_PATH = "/home/felix/PycharmProjects/docstring_generator/examples/mixed_before.py";

const std::string PY_TAB = "    ";

auto replaceAll = [](std::string& str_, const std::string& original, const std::string& new_){
    while(str_.find(original) != std::string::npos)
    {
        str_.replace(str_.find(original), original.size(), new_);
    }
};

enum class ParameterKind
{
    ARG,
    POS_ONLY,
    KW_ONLY,
    VARIADIC_ARG,
    KEYWORD_ARG,
};

enum class DocstringFormatStyle
{
    reST,
    GOOGLE,
    NUMPY
};

ParameterKind from_str(const std::string &kind)
{
    if (kind == "Argument")
    {
        return ParameterKind::ARG;
    }
    else if (kind == "Positional only argument")
    {
        return ParameterKind::POS_ONLY;
    }
    else if (kind == "Keyword only argument")
    {
        return ParameterKind::KW_ONLY;
    }
    else if (kind == "Variadic arguments")
    {
        return ParameterKind::VARIADIC_ARG;
    }
    else if (kind == "Keyword arguments")
    {
        return ParameterKind::KEYWORD_ARG;
    }
    
    return ParameterKind::ARG;
}

std::ostream& operator<<(std::ostream &out, ParameterKind const &obj) noexcept
{
    switch (obj)
    {
        case ParameterKind::ARG:
            return out << "Argument";
        case ParameterKind::POS_ONLY:
            return out << "Positional only argument";
        case ParameterKind::KW_ONLY:
            return out << "Keyword only argument";
        case ParameterKind::VARIADIC_ARG:
            return out << "Variadic arguments";
        case ParameterKind::KEYWORD_ARG:
            return out << "Keyword arguments";
    }

    return out;
}

std::string remove_trailing_whitespace(std::string &txt)
{
    // remove trailing whitespaces
    for (size_t idx = txt.size() - 1; idx > 0; --idx)
    {
        if (!std::isspace(txt[idx]))
        {
            return txt.substr(0, idx + 1);
        }
    }
    
    return "";
}

std::string remove_whitespace(const std::string &txt)
{
    // remove prefix whitespaces
    for (size_t idx = 0; idx < txt.size(); ++idx)
    {
        if (!std::isspace(txt[idx]))
        {
            return txt.substr(idx);
        }
    }
    
    return "";
}

struct FunctionParameter
{
    std::string name;
    std::string default_value;
    std::string type;
    ParameterKind kind;
    uint line_no;
    std::string description;
    
    void update_description(const std::string &descr, DocstringFormatStyle &formatStyle)
    {
        switch (formatStyle)
        {
            case DocstringFormatStyle::reST:
                update_rest_description(descr);
                break;
            case DocstringFormatStyle::GOOGLE:
            case DocstringFormatStyle::NUMPY:
                update_description(descr);
                break;
        }
    }
    
    void update_description(const std::string &descr)
    {
        std::stringstream sstream;
        sstream << kind;
        
        auto kind_name = sstream.str();
        auto start_pos = descr.find(kind_name) + kind_name.size() + 2;
        auto end_pos = descr.size();
        
        if (!default_value.empty())
        {
            end_pos = descr.find("default") - 2;
        }
        
        auto new_description = descr.substr(start_pos, end_pos - start_pos);
        
        description = remove_trailing_whitespace(new_description);
    }
    
    void update_rest_description(const std::string &descr)
    {
        std::stringstream sstream;
        
        auto param_name = ":param " + name + ":";
        auto type_param = ":type " + name + ":";
        auto kind_param = ":kind " + name + ":";
        auto start_pos = descr.find(param_name) + param_name.size();
        size_t end_pos = 0;
        
        if (!default_value.empty())
        {
            end_pos = descr.find("(default is") - 2;
        }
        else if (!type.empty())
        {
            end_pos = descr.find(type_param) - 2;
        }
        else
        {
            end_pos = descr.find(kind_param) - 2;
        }
        
        if ((end_pos + 2) < std::string::npos)
        {
            auto new_description = descr.substr(start_pos, end_pos - start_pos);
            description = remove_whitespace(remove_trailing_whitespace(new_description));
        }
    }
};

struct FunctionReturn
{
    std::string type;
    uint line_no;
    std::string description;
};

struct FunctionDocstring
{
    std::string docstring;
    uint start_line;
    uint end_line;
};

struct FunctionInfo
{
    uint offset;
    std::string name;
    FunctionDocstring docstring;
    FunctionReturn returns;
    std::vector<FunctionParameter> args {};
    
    int get_file_write_position()
    {
        if (!docstring.docstring.empty())
        {
            return docstring.start_line;
        }
        
        if (returns.line_no > 0)
        {
            return static_cast<int>(returns.line_no + 1);
        }
        else if (!args.empty())
        {
            return static_cast<int>(args[args.size() - 1].line_no + 1);
        }
        
        return 0;
    }
    
    void update_descriptions(DocstringFormatStyle &formatStyle)
    {
        for (size_t idx = 0; idx < args.size(); ++idx)
        {
            size_t start_pos;
            
            if (formatStyle == DocstringFormatStyle::reST)
            {
                start_pos = docstring.docstring.find(":param " + args[idx].name + ":");
            }
            else
            {
                start_pos = docstring.docstring.find(args[idx].name);
            }
            
            if (start_pos < std::string::npos)
            {
                uint end_pos = docstring.docstring.size() - 1;
                
                if (idx < args.size() - 1)
                {
                    end_pos = docstring.docstring.find(args[idx + 1].name);
                }
                else if (idx == args.size() - 1 && docstring.docstring.find("Returns") < std::string::npos)
                {
                    end_pos = docstring.docstring.find("Returns") - 1;
                }
                
                std::string part_of_interest = docstring.docstring.substr(start_pos, end_pos - start_pos);
                args[idx].update_description(part_of_interest, formatStyle);
            }
        }
    }
};

struct DocstringFormat
{
    FunctionInfo functionInfo;
    
    virtual std::string docstringArgs() noexcept = 0;
    virtual std::string docstringReturn() noexcept = 0;
    
    virtual void check_current_docstring() noexcept = 0;
    
    virtual ~DocstringFormat() = default;
    
    [[ nodiscard ]] std::string docstring() noexcept
    {
        std::stringstream sstream;
        auto current_pytab = get_tabs();
        
        sstream << current_pytab << R"(""")";
        if (functionInfo.docstring.docstring.empty())
        {
            sstream << "\n";
        }
        sstream << docstringArgs();
        sstream << docstringReturn();
        
        sstream << current_pytab << R"(""")";
        if (functionInfo.docstring.docstring.empty())
        {
            sstream << "\n";
        }
        
        return sstream.str();
    }
    
    [[ nodiscard ]] std::string get_tabs() noexcept
    {
        auto current_py_tab = PY_TAB;
        for (uint idx = 0; idx < (functionInfo.offset / 4); ++idx)
        {
            current_py_tab += PY_TAB;
        }
        
        return current_py_tab;
    }
};

struct GoogleDocstring : DocstringFormat
{
    void check_current_docstring() noexcept override
    {
        auto current_py_tab = get_tabs();
        auto google_args_begin = functionInfo.docstring.docstring.find("Args:");
        
        if (google_args_begin < std::string::npos)
        {
            functionInfo.docstring.docstring = functionInfo.docstring.docstring.substr(0, google_args_begin - (current_py_tab.size() + 1));
            functionInfo.docstring.end_line = functionInfo.docstring.start_line + google_args_begin;
        }
    }
    
    std::string docstringArgs() noexcept override
    {
        std::stringstream sstream;
        auto current_py_tab = get_tabs();
        
        if (!functionInfo.docstring.docstring.empty())
        {
            sstream << functionInfo.docstring.docstring;
            sstream << "\n";
        }
        
        if (PY_TAB != current_py_tab)
        {
            sstream << current_py_tab;
            current_py_tab += PY_TAB;
        }
        else
        {
            sstream << PY_TAB;
            current_py_tab = PY_TAB + PY_TAB;
        }
        
        sstream << "Args:\n";
        std::for_each(functionInfo.args.begin(), functionInfo.args.end(),
        [&sstream, &current_py_tab](const FunctionParameter &val){
            sstream << current_py_tab << val.name;
            if (!val.type.empty())
            {
                sstream << " (" << val.type;
                if (!val.default_value.empty())
                {
                    sstream << ", optional";
                }
                sstream << ")";
            }
            
            if (val.description.empty())
            {
                sstream << " : " << val.kind << "\n";
            }
            else
            {
                sstream << " : " << val.kind  << ". " << val.description << "\n";
            }
            
            if (!val.default_value.empty())
            {
                sstream << current_py_tab << PY_TAB << "(default is " << val.default_value << ")\n";
            }
        });
        
        return sstream.str();
    }
    
    std::string docstringReturn() noexcept override
    {
        std::stringstream sstream;
        auto current_py_tab = get_tabs();
        
        sstream << "\n";
        
        if (!functionInfo.returns.description.empty() || !functionInfo.returns.type.empty())
        {
            if (PY_TAB != current_py_tab)
            {
                sstream << PY_TAB;
            }
            else
            {
                sstream << PY_TAB;
                current_py_tab = PY_TAB + PY_TAB;
            }
            
            sstream << "Returns:\n";
            sstream << current_py_tab;
            if (!functionInfo.returns.type.empty())
            {
                sstream << "( " << functionInfo.returns.type << " ) : ";
            }
            sstream << functionInfo.returns.description << "\n";
            
            sstream << "\n";
        }
        
        return sstream.str();
    }
};

struct reStructuredDocstring : DocstringFormat
{
    void check_current_docstring() noexcept override
    {
        auto current_py_tab = get_tabs();
        auto rest_args_begin = functionInfo.docstring.docstring.find(":param");
        
        if (rest_args_begin < std::string::npos)
        {
            functionInfo.docstring.docstring = functionInfo.docstring.docstring.substr(0, rest_args_begin - (current_py_tab.size() + 1));
            functionInfo.docstring.end_line = functionInfo.docstring.start_line + rest_args_begin;
        }
    }
    
    std::string docstringArgs() noexcept override
    {
        std::stringstream docstream;
        
        auto current_py_tab = get_tabs();
        
        if (!functionInfo.docstring.docstring.empty())
        {
            docstream << functionInfo.docstring.docstring;
            docstream << "\n";
        }
        
        std::for_each(functionInfo.args.begin(), functionInfo.args.end(),
        [&docstream, &current_py_tab](const FunctionParameter &val)
        {
            
            docstream << current_py_tab << ":param " << val.name << ":";
            
            if (!val.description.empty())
            {
                docstream << " " << val.description;
            }
            
            docstream << "\n";
            
            if (!val.default_value.empty())
            {
                docstream << current_py_tab << PY_TAB << "(default is " << val.default_value << ")\n";
            }
            
            if (!val.type.empty())
            {
                docstream << current_py_tab << ":type " << val.name << ": " << val.type;
                if (!val.default_value.empty())
                {
                    docstream << ", optional";
                }
                
                docstream << "\n";
            }
            else if (val.type.empty() && !val.default_value.empty())
            {
                docstream << current_py_tab << ":type " << val.name << ": optional\n";
            }
            
            docstream << current_py_tab << ":kind " << val.name << ": " << val.kind << "\n";
            
        });
        
        return docstream.str();
    }
    
    std::string docstringReturn() noexcept override
    {
        std::stringstream sstream;
        auto current_py_tab = get_tabs();
        
        if (!functionInfo.returns.description.empty())
        {
            sstream << current_py_tab << ":returns:" << functionInfo.returns.description << "\n";
        }
        if (!functionInfo.returns.type.empty())
        {
            sstream << current_py_tab << ":rtype:" << functionInfo.returns.type << "\n";
        }
        return sstream.str();
    }
};

struct NumpyDocstring : DocstringFormat
{
    void check_current_docstring() noexcept override
    {
        auto current_py_tab = get_tabs();
        auto numpy_args_begin = functionInfo.docstring.docstring.find("Parameters");

        if (numpy_args_begin < std::string::npos)
        {
            functionInfo.docstring.docstring = functionInfo.docstring.docstring.substr(0, numpy_args_begin - (current_py_tab.size() + 1));
            functionInfo.docstring.end_line = functionInfo.docstring.start_line + numpy_args_begin;
        }
    }

    std::string docstringArgs() noexcept override
    {
        std::stringstream docstream;
        auto current_py_tab = get_tabs();

        if (!functionInfo.docstring.docstring.empty())
        {
            docstream << functionInfo.docstring.docstring;
            docstream << "\n";
        }

        docstream << current_py_tab << "Parameters\n";
        docstream << current_py_tab << "----------\n";
        std::for_each(functionInfo.args.begin(), functionInfo.args.end(),
                      [&docstream, &current_py_tab](const FunctionParameter &val)
        {
            std::stringstream sstream;

            sstream << current_py_tab << val.name;
            sstream << " :" ;

            if (!val.type.empty())
            {
              sstream << " " << val.type;

              if (!val.default_value.empty())
              {
                  sstream << ", optional";
              }
            }

            if (!val.default_value.empty())
            {
                if (!val.type.empty())
                {
                    sstream << ", ";
                }
                sstream << "default: " << val.default_value;
            }

            sstream << " [" << val.kind << "]";

            if (!val.description.empty())
            {
                sstream << "\n";
                sstream << current_py_tab << PY_TAB << remove_whitespace(val.description);
            }

            auto docstring = sstream.str();
            docstring = remove_trailing_whitespace(docstring);

            docstream << docstring << "\n";
        });

        return docstream.str();
    }

    std::string docstringReturn() noexcept override
    {
        std::stringstream sstream;
        auto current_py_tab = get_tabs();

        if (!functionInfo.returns.type.empty())
        {
            sstream << "\n";
            sstream << current_py_tab << "Returns\n";
            sstream << current_py_tab << "-------\n";

            sstream << current_py_tab << functionInfo.returns.type;
        }

        if (!functionInfo.returns.description.empty())
        {
            sstream << "\n";
            sstream << current_py_tab << PY_TAB << functionInfo.returns.description;
        }

        sstream << "\n";

        return sstream.str();
    }
};

FunctionDocstring get_docstring(py::object &obj, py::module &ast_module) noexcept;
std::string parse_ast_constant(py::object &obj, py::module &ast_module) noexcept;
std::string parse_ast_name(py::object &obj, py::module &ast_module) noexcept;
std::string parse_ast_subscript(py::object &obj, py::module &ast_module) noexcept;
std::string parse_ast_attribute(py::object &obj, py::module &ast_module) noexcept;
std::string parse_ast_tuple_list(py::object &obj, py::module &ast_module) noexcept;
std::string parse_ast_binop(py::object &obj, py::module &ast_module) noexcept;
std::string parse_ast_obj(py::object &obj, py::module &ast_module) noexcept;
void write_to_file_position(std::vector<FunctionInfo> &&infos,
                            const std::string &file_path,
                            DocstringFormatStyle &formatStyle) noexcept;

FunctionDocstring get_docstring(const py::object obj, py::module &ast_module) noexcept
{
    FunctionDocstring functionDocstring {};
    
    py::list body_list = py::getattr(obj, "body");
    py::object body = body_list[0];
    
    if (py::hasattr(body, "value"))
    {
        py::object body_val = py::getattr(body, "value");
        
        if (py::isinstance(body_val, ast_module.attr("Constant")))
        {
            try
            {
                functionDocstring.docstring = py::cast<std::string>(py::getattr(body_val, "value"));
            }
            catch (std::exception &err)
            {
                return functionDocstring;
            }
            
            functionDocstring.start_line = py::cast<uint>(py::getattr(body_val, "lineno"));
            functionDocstring.end_line = py::cast<uint>(py::getattr(body_val, "end_lineno"));
        }
    }
    
    return functionDocstring;
}

std::string parse_ast_constant(py::object &obj, py::module &ast_module) noexcept
{
    return py::cast<std::string>(py::repr(py::getattr(obj, "value")));
}

std::string parse_ast_name(py::object &obj, py::module &ast_module) noexcept
{
    return py::cast<std::string>(py::getattr(obj, "id"));
}

std::string parse_ast_subscript(py::object &obj, py::module &ast_module) noexcept
{
    auto value = py::getattr(obj, "value");
    auto slice = py::getattr(obj, "slice");
    return parse_ast_obj(value, ast_module) + "[" + parse_ast_obj(slice, ast_module) + "]";
}

std::string parse_ast_attribute(py::object &obj, py::module &ast_module) noexcept
{
    std::string result {};
    
    py::object value = py::getattr(obj, "value");
    if (py::isinstance(value, ast_module.attr("Attribute")))
    {
        result = parse_ast_attribute(value, ast_module);
    }
    else if (py::isinstance(value, ast_module.attr("Name")))
    {
        result = parse_ast_name(value, ast_module);
    }
    
    return result + "." + py::cast<std::string>(py::getattr(obj, "attr"));
}

std::string parse_ast_tuple_list(py::object &obj, py::module &ast_module) noexcept
{
    std::string result {};
    py::list elts = py::getattr(obj, "elts");
    
    for (py::handle item: elts)
    {
        py::object obj = py::reinterpret_borrow<py::object>(item);
        result += parse_ast_obj(obj, ast_module) + ", ";
    }
    
    return result.substr(0, result.size() - 2);
}

std::string parse_ast_binop(py::object &obj, py::module &ast_module) noexcept
{
    auto left = py::getattr(obj, "left");
    auto right = py::getattr(obj, "right");
    
    std::string result = "Union["
            + parse_ast_obj(left, ast_module)
            + ", "
            + parse_ast_obj(right, ast_module)
            + "]";
    return result;
}

std::string parse_ast_obj(py::object &obj, py::module &ast_module) noexcept
{
    if (obj.is_none())
    {
        return "";
    }
    
    if (py::isinstance(obj, ast_module.attr("Attribute")))
    {
        return parse_ast_attribute(obj, ast_module);
    }
    else if (py::isinstance(obj, ast_module.attr("Constant")))
    {
        return parse_ast_constant(obj, ast_module);
    }
    else if (py::isinstance(obj, ast_module.attr("Name")))
    {
        return parse_ast_name(obj, ast_module);
    }
    else if (py::isinstance(obj, ast_module.attr("Subscript")))
    {
        return parse_ast_subscript(obj, ast_module);
    }
    else if (py::isinstance(obj, ast_module.attr("Tuple")))
    {
        return parse_ast_tuple_list(obj, ast_module);
    }
    else if (py::isinstance(obj, ast_module.attr("List")))
    {
        return parse_ast_tuple_list(obj, ast_module);
    }
    else if (py::isinstance(obj, ast_module.attr("BinOp")))
    {
        return parse_ast_binop(obj, ast_module);
    }
    else
    {
        return "";
    }
}

std::vector<FunctionParameter> generate_function_parameters(py::object &function_args, py::module &ast_module) noexcept
{
    std::vector<FunctionParameter> result {};
    
    py::list posonlyargs = py::getattr(function_args, "posonlyargs");
    py::list args_list = py::getattr(function_args, "args");
    py::object vararg = py::getattr(function_args, "vararg");
    py::list kwonlyargs = py::getattr(function_args, "kwonlyargs");
    py::list kw_defaults = py::getattr(function_args, "kw_defaults");
    py::object kwarg = py::getattr(function_args, "kwarg");
    py::list defaults = py::getattr(function_args, "defaults");
    
    size_t total_args = py::len(posonlyargs) + py::len(args_list) + py::len(kwonlyargs);
    if (!vararg.is_none())
    {
        ++total_args;
    }
    
    if (!kwarg.is_none())
    {
        ++total_args;
    }
    
    result.reserve(total_args);
    
    if (py::len(posonlyargs) > 0)
    {
        for (auto elem : posonlyargs)
        {
            py::object annotation = py::getattr(elem, "annotation");
            auto end_lineno = py::cast<uint>(py::getattr(elem, "end_lineno"));
            std::string annotation_str {};
            
            std::string parameter_name = py::cast<std::string>(py::getattr(elem, "arg"));
            
            if (!annotation.is_none())
            {
                annotation_str = parse_ast_obj(annotation, ast_module);
            }
            
            result.emplace_back(
                    FunctionParameter {
                        parameter_name,
                        "",
                        annotation_str,
                        ParameterKind::POS_ONLY,
                        end_lineno}
            );
        }
    }

    if (py::len(args_list) > 0)
    {
        for (auto elem : args_list)
        {
            py::object annotation = py::getattr(elem, "annotation");
            auto end_lineno = py::cast<uint>(py::getattr(elem, "end_lineno"));
            std::string annotation_str {};
    
            if (!annotation.is_none())
            {
                annotation_str = parse_ast_obj(annotation, ast_module);
            }
            
            std::string parameter_name = py::cast<std::string>(py::getattr(elem, "arg"));
            if (parameter_name == "self")
            {
                annotation_str = "object";
            }

            result.emplace_back(
                    FunctionParameter {
                        parameter_name,
                        "",
                        annotation_str,
                        ParameterKind::ARG,
                        end_lineno}
            );
        }
    }

    if (!vararg.is_none())
    {
        py::object annotation = py::getattr(vararg, "annotation");
        auto end_lineno = py::cast<uint>(py::getattr(vararg, "end_lineno"));
        std::string annotation_str {};
    
        if (!annotation.is_none())
        {
            annotation_str = parse_ast_obj(annotation, ast_module);
        }
        
        std::string parameter_name = py::cast<std::string>(py::getattr(vararg, "arg"));
        result.emplace_back(
                FunctionParameter {
                    parameter_name,
                    "",
                    annotation_str,
                    ParameterKind::VARIADIC_ARG,
                    end_lineno}
        );
    }

    if (py::len(kwonlyargs) > 0)
    {
        for (auto elem : kwonlyargs)
        {
            py::object annotation = py::getattr(elem, "annotation");
            auto end_lineno = py::cast<uint>(py::getattr(elem, "end_lineno"));
            std::string annotation_str {};
    
            if (!annotation.is_none())
            {
                annotation_str = parse_ast_obj(annotation, ast_module);
            }
            
            std::string parameter_name = py::cast<std::string>(py::getattr(elem, "arg"));
            result.emplace_back(
                    FunctionParameter {
                            parameter_name,
                            "",
                            annotation_str,
                            ParameterKind::KW_ONLY,
                            end_lineno}
            );
        }
    }

    if (!kwarg.is_none())
    {
        py::object annotation = py::getattr(kwarg, "annotation");
        auto end_lineno = py::cast<uint>(py::getattr(kwarg, "end_lineno"));
        std::string annotation_str {};
    
        if (!annotation.is_none())
        {
            annotation_str = parse_ast_obj(annotation, ast_module);
        }
        
        std::string parameter_name = py::cast<std::string>(py::getattr(kwarg, "arg"));
        result.emplace_back(
                FunctionParameter {
                        parameter_name,
                        "",
                        annotation_str,
                        ParameterKind::KEYWORD_ARG,
                        end_lineno}
        );
    }
    if (py::len(kw_defaults) > 0)
    {
        auto result_size = result.size();

        for (size_t idx = 0; idx < py::len(kw_defaults); ++idx)
        {
            auto obj_idx = py::len(kw_defaults) - 1 - idx;
            py::object item = kw_defaults[obj_idx];
            result.at(result_size - 1 - idx).default_value = parse_ast_obj(item, ast_module);
        }
    }
    
    if (py::len(defaults) > 0)
    {
        auto kw_defaults_len = py::len(kw_defaults);
        auto defaults_len = py::len(defaults);
        auto result_size = result.size();

        for (size_t idx = 0; idx < defaults_len; ++idx)
        {
            auto obj_idx = defaults_len - 1 - idx;
            auto result_idx = result_size - kw_defaults_len - 1 - idx;

            py::object item = defaults[obj_idx];
            
            try
            {
                result.at(result_idx).default_value = parse_ast_obj(item, ast_module);
            }
            catch (std::exception &err)
            {
                result.at(result_size - kw_defaults_len - 1 - idx).default_value = err.what() + std::to_string(result_idx) + " " + std::to_string(obj_idx);
            }
        }
    }
    
    return result;
}

std::string read_file(const std::string &file_path)
{
    std::ifstream file(file_path, std::ios::binary);
    std::string file_content {};
    
    if (file.is_open())
    {
        std::ostringstream outsstream {};
        outsstream << file.rdbuf();
        file_content = outsstream.str();
    }
    else
    {
        throw py::value_error(file_path + " is not a valid path.");
    }
    
    return file_content;
}

void get_docstring_arg_descr(FunctionInfo &functionInfo) noexcept
{
    if (functionInfo.docstring.docstring.empty())
    {
        return;
    }
    
    auto start = functionInfo.docstring.docstring.find('$');
    if (start < std::string::npos)
    {
        auto end = functionInfo.docstring.docstring.find('\n', start + 1);
        std::string description;
        
        if (end >= std::string::npos)
        {
            return;
        }
        
        try
        {
            description = functionInfo.docstring.docstring.substr(start, end - start);
        }
        catch (std::out_of_range &err)
        {
            return;
        }
        
        auto end_id = description.find(' ');
        int id = std::stoi(description.substr(1, end_id)) - 1;
        
        if (static_cast<size_t>(id) < functionInfo.args.size())
        {
            try
            {
                functionInfo.args[id].description = description.substr(end_id);
            }
            catch (std::out_of_range &err)
            {
                return;
            }
            
            auto reduce = functionInfo.offset > 0 ? 2 * functionInfo.offset + 2 : 4 + 2;
            
            try
            {
                functionInfo.docstring.docstring.erase(start - reduce,
                                                       end - start + reduce);
            }
            catch (std::out_of_range &err)
            {
                // no further action needed
            }
    
            get_docstring_arg_descr(functionInfo);
        }
    }
}

void parse_file(std::string &file_path, DocstringFormatStyle &formatStyle)
{
    std::string file_path_cache = file_path;
    py::module ast_module = py::module::import("ast");
    py::object generator_result = ast_module.attr("walk")(ast_module.attr("parse")(read_file(file_path)));
    py::iterator iter = py::iter(generator_result);
    
    std::vector<FunctionInfo> infos {};
    
    for (auto &obj : iter)
    {
        if (py::isinstance(obj, ast_module.attr("FunctionDef")))
        {
            auto name = py::cast<std::string>(py::getattr(obj, "name"));
            auto offset = py::cast<uint>(py::getattr(obj, "col_offset"));

            FunctionDocstring doc_str = get_docstring(py::reinterpret_borrow<py::object>(obj), ast_module);

            auto returns = py::getattr(obj, "returns");
            
            std::string return_str {};
            uint return_end_line_no = 0;

            if (!returns.is_none())
            {
                return_str = parse_ast_obj(returns, ast_module);
                return_end_line_no = py::cast<uint>(py::getattr(returns, "end_lineno"));
            }

            FunctionReturn functionReturn {
                return_str,
                return_end_line_no
            };

            py::object args = py::getattr(obj, "args");
            auto f_args = generate_function_parameters(args, ast_module);

            FunctionInfo functionInfo {
                offset,
                name,
                doc_str,
                functionReturn,
                f_args
            };
            
            get_docstring_arg_descr(functionInfo);
            functionInfo.update_descriptions(formatStyle);
            
            infos.emplace_back(functionInfo);
        }
    }
    
    write_to_file_position(std::move(infos), file_path, formatStyle);
}

void write_to_file_position(std::vector<FunctionInfo> &&infos,
                            const std::string &file_path,
                            DocstringFormatStyle &formatStyle) noexcept
{
    std::fstream file;
    
    auto lines = std::make_unique<std::vector<std::string>>();
    
    file.open(file_path, std::ios::in);
    
    for (std::string line; getline(file, line);)
    {
        lines->emplace_back(line);
    }
    
    file.close();
    
    std::reverse(infos.begin(), infos.end());
    
    for (auto &val : infos)
    {
        int start_pos = val.get_file_write_position() - 1;
        int end_pos = start_pos + 0;
        
        if (!val.docstring.docstring.empty())
        {
            end_pos = val.docstring.end_line;
        }
        
        if (start_pos < 0 || end_pos - 1 < 0)
        {
            break;
        }

        lines->erase(lines->begin() + start_pos, lines->begin() + end_pos);
    
        std::unique_ptr<DocstringFormat> docstring_format;
        
        switch (formatStyle)
        {
            case DocstringFormatStyle::reST:
                docstring_format = std::make_unique<reStructuredDocstring>();
                break;
            case DocstringFormatStyle::GOOGLE:
                docstring_format = std::make_unique<GoogleDocstring>();
                break;
            case DocstringFormatStyle::NUMPY:
                docstring_format = std::make_unique<NumpyDocstring>();
                break;
        }
    
        docstring_format->functionInfo = val;
        docstring_format->check_current_docstring();
        
        lines->insert(lines->begin() + start_pos, docstring_format->docstring());
    }
    
    file.open(file_path, std::ios::out);

    std::for_each(lines->begin(), lines->end(),
                  [&file](const auto &line)
    {
        file << line;
        if (line[line.size() - 1] != '\n')
        {
            file << "\n";
        }
    });
    
    file.close();
}


PYBIND11_MODULE(docstring_generator_ext, m)
{
    m.doc() = "pybind11 plugin to add automatically add docstring"; // optional module docstring
    m.def("parse_file", &parse_file, "A function that parses a file",
          py::arg("file_path"),
          py::arg("formatStyle"),
          "The file_path where automatically docstrings should be added.",
          "In which style should the Docstring be written.");
    
    py::enum_<DocstringFormatStyle>(m, "DocstringFormatStyle")
            .value("reST", DocstringFormatStyle::reST)
            .value("GOOGLE", DocstringFormatStyle::GOOGLE)
            .value("NUMPY", DocstringFormatStyle::NUMPY)
            .export_values();
}