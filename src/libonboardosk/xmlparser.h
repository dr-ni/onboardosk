#ifndef XMLPARSER_H
#define XMLPARSER_H

#include <memory>
#include <map>

class ContextBase;

enum class XMLNodeType
{
    ELEMENT,
    TEXT,
    UNSUPPORTED,
};

class XMLNode : public std::enable_shared_from_this<XMLNode>
{
    public:
        using Ptr = std::shared_ptr<XMLNode>;
        virtual ~XMLNode() = default;

        Ptr getptr() {
            return std::enable_shared_from_this<XMLNode>::shared_from_this();
        }

        virtual XMLNodeType get_node_type() = 0;
        virtual std::string get_tag_name() = 0;
        virtual bool has_attribute(const std::string& s) = 0;
        virtual void get_attributes(std::map<std::string, std::string>& attributes) = 0;
        virtual std::string get_attribute(const std::string& s) = 0;

        virtual bool get_attribute(const std::string& s, std::string& result)
        {
            if (has_attribute(s))
            {
                result = get_attribute(s);
                return true;
            }
            return false;
        }

        virtual std::string get_text() = 0;

        virtual Ptr get_next_sibling() = 0;
        virtual Ptr get_first_child() = 0;

        template< typename F >
        void for_each_element_by_tag_name(const std::string& tag,
                                          const F& func)
        {
            for_each_element_by_tag_name(tag, func, getptr());
        }

    private:
        template< typename F >
        void for_each_element_by_tag_name(const std::string& tag,
                                          const F& func, const Ptr& node)
        {
            for (auto cur_node = node->get_first_child(); cur_node;
                 cur_node = cur_node->get_next_sibling())
            {
                if (cur_node->get_node_type() == XMLNodeType::ELEMENT)
                {
                    if (cur_node->get_tag_name() == tag)
                        func(cur_node);

                    for_each_element_by_tag_name(tag, func, cur_node);
                }
            }
        }
};

typedef std::shared_ptr<XMLNode> XMLNodePtr;


class XMLParser
{
    public:
        virtual ~XMLParser() = default;
        virtual XMLNodePtr parse_file_utf8(const std::string& fn) = 0;

        static std::unique_ptr<XMLParser> make_xml2(const ContextBase& context);
};

#endif // XMLPARSER_H

