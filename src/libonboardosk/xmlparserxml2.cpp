
#include <memory>
#include <string>

//extern "C" {
#include <libxml/tree.h>
#include <libxml/parser.h>
//}

#include "xmlparser.h"
#include "onboardoskglobals.h"

using namespace std;


class XMLNodeXML2 : public XMLNode
{
    public:
        XMLNodeXML2(xmlNode* node) :
            m_node(node)
        {}

        virtual ~XMLNodeXML2() = default;

        virtual bool has_attribute(const std::string& s) override
        {
            return xmlHasProp(m_node,
                              reinterpret_cast<const xmlChar*>(s.c_str()))
                   != nullptr;
        }

        virtual std::string get_attribute(const std::string& s) override
        {
            string result;
            xmlChar* ret = xmlGetProp(m_node,
                                      reinterpret_cast<const xmlChar*>(s.c_str()));
            if (ret)
            {
                result = reinterpret_cast<char*>(ret);
                xmlFree(ret);
            }
            return result;
        }

        virtual XMLNode::Ptr get_next_sibling() override
        {
            if (m_node->next)
                return make_shared<XMLNodeXML2>(m_node->next);
            return {};
        }

        virtual XMLNode::Ptr get_first_child() override
        {
            if (m_node->children)
                return make_shared<XMLNodeXML2>(m_node->children);
            return {};
        }

        virtual XMLNodeType get_node_type() override
        {
            switch (m_node->type)
            {
                case XML_ELEMENT_NODE: return XMLNodeType::ELEMENT;
                case XML_TEXT_NODE: return XMLNodeType::TEXT;
                default: return XMLNodeType::UNSUPPORTED;
            }
        }

        virtual std::string get_tag_name() override
        {
            return reinterpret_cast<const char*>(m_node->name);
        }

        virtual void get_attributes(std::map<std::string, std::string>& attributes) override
        {
            attributes.clear();

            auto prop = m_node->properties;
            while (prop)
            {
                std::string name = reinterpret_cast<const char*>(prop->name);
                attributes[name] = get_attribute(name);
                prop = prop->next;
            }
        }

        virtual std::string get_text() override
        {
            if (m_node->content)
                return reinterpret_cast<const char*>(m_node->content);
            if (m_node->children && m_node->children->content)
                return reinterpret_cast<const char*>(m_node->children->content);
            return {};
        }

     private:
        xmlNode* m_node;
};

class XMLParserXML2Impl : public XMLParser, public ContextBase
{
    public:
        XMLParserXML2Impl(const ContextBase& context) :
            ContextBase(context)
        {
            LIBXML_TEST_VERSION    // check if API matches the shared library
        }
        virtual ~XMLParserXML2Impl()
        {
            xmlFreeDoc(m_doc);
            xmlCleanupParser(); // free globals :/
        }

        // public interface
        virtual XMLNodePtr parse_file_utf8(const std::string& fn) override;

    private:
        xmlDocPtr m_doc{};
        xmlNodePtr m_root_node{};
};

std::unique_ptr<XMLParser> XMLParser::make_xml2(const ContextBase& context)
{
    return make_unique<XMLParserXML2Impl>(context);
}


XMLNodePtr XMLParserXML2Impl::parse_file_utf8(const std::string& fn)
{
    m_doc = xmlReadFile(fn.c_str(), "UTF-8", 0);
    if (m_doc == NULL)
    {
        return {};
    }
    m_root_node = xmlDocGetRootElement(m_doc);
    return make_shared<XMLNodeXML2>(m_root_node);
}

