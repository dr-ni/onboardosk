#ifndef CONFIGOBJECT_H
#define CONFIGOBJECT_H

#include <cassert>
#include <functional>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "onboardoskglobals.h"
#include "signalling.h"
#include "exception.h"

class GSKeyBase;
struct _GSettings;
typedef struct _GSettings GSettings;


class ConfigObject : public ContextBase
{
    public:
        using Super = ContextBase;
        using This = ConfigObject;
        using Ptr = std::shared_ptr<This>;
        using GSKeyMap = std::map<std::string, GSKeyBase*>;

        ConfigObject(const ContextBase& context,
                     const std::string& schema,
                     const std::string& section);
        ConfigObject(ConfigObject* parent,
                     const std::string& schema,
                     const std::string& section);
        virtual ~ConfigObject();

        std::string get_schema() {return m_schema;}

        void sync();
        void set_modified(bool modified);

        void delay();
        void apply();

        void read_value(const std::string& key_name, bool& value);
        void write_value(const std::string& key_name, bool value, const std::string& type_string="b");

        void read_value(const std::string& key_name, int& value);
        void write_value(const std::string& key_name, int value, const std::string& type_string="i");

        void read_value(const std::string& key_name, double& value);
        void write_value(const std::string& key_name, double value, const std::string& type_string="d");

        void read_value(const std::string& key_name, std::string& value);
        void write_value(const std::string& key_name, const std::string& value, const std::string& type_string="s");

        int read_enum(const std::string& key_name);
        void write_enum(const std::string& key_name, int value);

        void read_value(const std::string& key_name, std::vector<double>& value);
        void write_value(const std::string& key_name, const std::vector<double>& value, const std::string& type_string="ad");

        void read_value(const std::string& key_name, std::vector<std::string>& value);
        void write_value(const std::string& key_name, const std::vector<std::string>& value, const std::string& type_string="as");

        void read_value(const std::string& key_name, std::vector<std::vector<std::string>>& value);
        void write_value(const std::string& key_name, const std::vector<std::vector<std::string>>& value, const std::string& type_string="aas");

        void read_value(const std::string& key_name, std::map<int, int>& value);
        void write_value(const std::string& key_name, const std::map<int, int>& value, const std::string& type_string="a{ii}");

        void read_value(const std::string& key_name, std::map<std::string, std::string>& value);
        void write_value(const std::string& key_name, const std::map<std::string, std::string>& value, const std::string& type_string="a{ss}");

        void add_key(GSKeyBase* gskey)
        {
            m_gskeys.emplace_back(gskey);
        }

        void on_change_event(const char* key_name);

    protected:
        // init GSKey values from gsettings
        void read_all_keys();

    private:
        void construct();

    protected:
        ConfigObject* m_parent{};
        std::vector<ConfigObject*> m_children;

    private:
        GSettings* m_settings{};
        std::vector<GSKeyBase*> m_gskeys;
        std::string m_schema;
        std::string m_section;    // used to be sysdef section

        // True if anything was written, cleared with sync().
        bool m_modified;

};





// optionaly pack to and unpack from a different type in gsettings vs. the property's type
template<typename TProp, typename TSetting>
class GSPacker
{
    public:
        using UnpackFunc = std::function<TProp(const TSetting&)>;
        using PackFunc = std::function<TSetting(const TProp&)>;

        //static UnpackFunc default_unpack_func = [](const TProp& value) {return value;};
        //static PackFunc default_pack_func = [](const TSetting& value) {return value;};
        TSetting default_pack_func(const TProp& value) {return value;}

        GSPacker<TProp, TSetting>(const std::string& type_string={},
                                  UnpackFunc unpack_func={},
                                  PackFunc pack_func={}) :
            m_type_string(type_string),
            m_unpack_func(unpack_func),
            m_pack_func(pack_func)
        {
            set_default_funcs();
        }

        // provide default unpack/pack functions for simple types, e.g.
        // for TProp=double, TSetting=int
        template <typename U = TProp>
        typename std::enable_if<std::is_pod<U>::value &&
                                std::is_pod<TSetting>::value>::type
        set_default_funcs()
        {
            if (!m_unpack_func)
                m_unpack_func = [](const TSetting& val) -> TProp
                {
                    return static_cast<TProp>(val);
                };

            if (!m_pack_func)
                m_pack_func = [](const TProp& val) -> TSetting
                {
                    return static_cast<TSetting>(val);
                };
        }

        // no default unpack/pack functions for all other types
        template <typename U = TProp>
        typename std::enable_if<!(std::is_pod<U>::value &&
                                  std::is_pod<TSetting>::value)>::type
        set_default_funcs()
        {
        }

    public:
        std::string m_type_string;
        UnpackFunc m_unpack_func{};
        PackFunc m_pack_func{};
};


class GSKeyBase : public ContextBase
{
    public:
        using Super = ContextBase;

        GSKeyBase(const ContextBase& context) :
            Super(context)
        {}
        virtual ~GSKeyBase() = default;

        virtual void read() = 0;
        virtual void write() = 0;
        virtual void on_change_event() = 0;

        const std::string& get_key_name()
        {
            return m_key_name;
        }

    protected:
        bool can_log_read();
        bool can_log_write();
        void log_read(const std::string& msg);
        void log_write(const std::string& msg);
        void log_exception(const class Exception& ex, const std::string& msg);

    protected:
        ConfigObject* m_config_object{};
        std::string m_key_name;
};

// Key-value tuple for ConfigObject
// Associates python ConfigObject properties with gsettings keys,
// and command line options.
template<typename TProp, typename TSetting=TProp>
class GSKey : public GSKeyBase
{
    public:
        using Super = GSKeyBase;
        using EnumValues = const std::map<std::string, TProp>;

        GSKey<TProp, TSetting>(ConfigObject* config_object,
                    const std::string& key_name,
                    const TProp& default_value={},
                    GSPacker<TProp, TSetting> packer={},
                    const EnumValues& enum_values={}) :
            Super(*config_object),
            m_default_value(default_value),
            m_packer(packer),
            m_enum_values(enum_values),
            m_value(default_value)
        {
            m_config_object = config_object;
            m_key_name = key_name;
            config_object->add_key(this);
        }

        operator TProp&()
        {
            return get();
        }
        operator const TProp&() const
        {
            return get();
        }

        TProp& operator=(const TProp &value)
        {
            set(value);
            return m_value;
        }

        TProp& get()
        {
            return m_value;
        }
        const TProp& get() const
        {
            return m_value;
        }

        void set(TProp value)
        {
            if (m_value != value)
            {
                m_value = value;
                try
                {
                    write();
                }
                catch (const Exception& ex)
                {
                    log_exception(ex,
                        "error writing key '" + get_key_name() + "'" );
                }
                // The write notification will not emit because
                // the value is already the same as the written one.
                // Do it here to make sure notifications arrive.
                changed.emit();
            }
        }

        // read from storage (gsettings)
        virtual void read() override
        {
            // Make sure whatever we wrote to gsettings
            // has arrived and we won't get stale data.
            // Happend in unit tests for worldlist-buttons.
            m_config_object->sync();

            // Is property type an enum?
            if constexpr (std::is_enum<TProp>::value)
            {
                assert(!m_enum_values.empty());   // don't forget to add the enum map to the property
                m_value = static_cast<TProp>(m_config_object->read_enum(m_key_name));
            }
            else
            {
                // Same type for property and gsettings data?
                if constexpr(std::is_same<TProp, TSetting>::value)
                {
                    m_config_object->read_value(m_key_name, m_value);
                }
                // Different type for property and gsettings data!
                else
                {
                    TSetting packed;
                    m_config_object->read_value(m_key_name, packed);
                    assert(m_packer.m_unpack_func);  // unpack/pack functions missing?
                    m_value = m_packer.m_unpack_func(packed);
                }
            }

            if (can_log_read())
            {
                std::stringstream ss;
                ss << std::boolalpha
                   << "GSKey::read:"
                   << " " << this->m_config_object->get_schema()
                   << " " << get_key_name()
                   << " " << m_value;
                log_read(ss.str());
            }
        }

        // write to storage (gsettings)
        virtual void write() override
        {
            // Mark modified, requesting sync() to run
            m_config_object->set_modified(true);

            if (can_log_write())
            {
                std::stringstream ss;
                ss << std::boolalpha
                   << "GSKey::write:"
                   << " " << this->m_config_object->get_schema()
                   << " " << get_key_name()
                   << " " << m_value;
                log_write(ss.str());
            }

            // Is property type an enum?
            if constexpr (std::is_enum<TProp>::value)
            {
                assert(!m_enum_values.empty());   // don't forget to add the enum map to the property
                m_config_object->write_enum(m_key_name, m_value);
            }
            else
            {
                // Same type for property and gsettings data?
                if constexpr(std::is_same<TProp, TSetting>::value)
                {
                    m_config_object->write_value(m_key_name, m_value);
                }
                // Different type for property and gsettings data!
                else
                {
                    assert(m_packer.m_pack_func);  // unpack/pack functions missing?
                    TSetting packed = m_packer.m_pack_func(m_value);
                    m_config_object->write_value(m_key_name, packed);
                }
            }
        }

        virtual void on_change_event()
        {
            auto old_value = std::move(m_value);
            read();
            if (old_value != m_value)
                changed.emit();
        }

    public:
        DEFINE_SIGNAL(<>, changed, this);

    private:
        TProp m_default_value{};
        const GSPacker<TProp, TSetting> m_packer;
        EnumValues m_enum_values;

        TProp m_value;
        bool m_none{true};
};

#endif // CONFIGOBJECT_H
