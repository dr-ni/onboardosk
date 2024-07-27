#include <gio/gio.h>

#include "tools/glib_helpers.h"
#include "tools/logger.h"
#include "tools/string_helpers.h"

#include "configobject.h"
#include "exception.h"


bool GSKeyBase::can_log_read()
{
    return logger()->can_log(LogLevel::TRACE);
}
void GSKeyBase::log_read(const std::string& msg)
{
    LOG_TRACE << msg;
}

bool GSKeyBase::can_log_write()
{
    return logger()->can_log(LogLevel::WARNING);
    return logger()->can_log(LogLevel::DEBUG);
}
void GSKeyBase::log_write(const std::string& msg)
{
    LOG_WARNING << msg;
    LOG_DEBUG << msg;
}

void GSKeyBase::log_exception(const Exception& ex, const std::string& msg)
{
    LOG_ERROR << msg
              << ": " << ex.what();
}


static gboolean on_gsettings_change_event (GSettings *settings,
                                           gpointer   keys,
                                           gint       n_keys,
                                           gpointer   user_data)
{
    (void)settings;

    ConfigObject* this_ = reinterpret_cast<ConfigObject*>(user_data);
    for (gint i=0; i<n_keys; i++)
    {
        GQuark* quarks = reinterpret_cast<GQuark*>(keys);
        const gchar* name = g_quark_to_string(quarks[i]);
        this_->on_change_event(name);
    }
    return TRUE;  // done, we don't need the "changed" signal
}

ConfigObject::ConfigObject(const ContextBase& context,
                           const std::string& schema,
                           const std::string& section) :
                           ContextBase(context),
                           m_parent(nullptr),
                           m_schema(schema),
                           m_section(section)
{
    construct();
}

ConfigObject::ConfigObject(ConfigObject* parent,
                           const std::string& schema,
                           const std::string& section) :
    ContextBase(*parent),
    m_parent(parent),
    m_schema(schema),
    m_section(section)
{
    construct();
}

void ConfigObject::construct()
{
    //gtk_init_check();
    //m_settings = g_settings_new("org.gtk.test");
    m_settings = g_settings_new(m_schema.c_str());
    if (!m_settings)
        throw SchemaException(sstr()
            << "g_gsettings_new failed for " << repr(m_schema) << "."
            << " Is this GSettings schema installed?");

    g_signal_connect (m_settings, "change_event", G_CALLBACK (on_gsettings_change_event), this);
}

ConfigObject::~ConfigObject()
{
    if (m_settings)
    {
         g_signal_handlers_disconnect_by_data (m_settings, this);
         g_object_unref(m_settings);
    }
}

void ConfigObject::on_change_event(const char* key_name)
{
    for(auto& gskey : m_gskeys)
    {
        if (gskey->get_key_name() == key_name)
        {
            gskey->on_change_event();
            break;
        }
    }
}

void ConfigObject::sync()
{
    if (m_modified &&
        m_settings)
    {
        g_settings_sync();
        set_modified(false);
    }
}

void ConfigObject::set_modified(bool modified)
{
    m_modified = modified;
}

void ConfigObject::delay()
{
    if (m_settings)
        g_settings_delay(m_settings);
}

void ConfigObject::apply()
{
    if (m_settings)
        g_settings_apply(m_settings);
}

// init propertiy values from gsettings
void ConfigObject::read_all_keys()
{
    for (auto gskey : m_gskeys)
    {
        try {
            gskey->read();
        } catch (const Exception& ex) {
            LOG_ERROR << "error reading key " << repr(gskey->get_key_name())
                      << ": " << ex.what();
        }
    }

    for (auto child  : this->m_children)
        child->read_all_keys();
}

static void variant_to_value(GVariant* v, int& value)
{
    value = g_variant_get_int32(v);
}
static void variant_to_value(GVariant* v, double& value)
{
    value = g_variant_get_double(v);
}
static void variant_to_value(GVariant* v, std::string& value)
{
    value = g_variant_get_string(v, nullptr);
}
static void variant_to_value(GVariant* v, std::vector<std::string>& value)
{
    gsize length;
    auto a = g_variant_get_strv(v, &length);
    for (gsize i=0; i<length; i++)
        value.emplace_back(a[i]);
    g_free(a);
}

static GVariant* value_to_variant(double value)
{
    return g_variant_new_double(value);
}

static GVariant* value_to_variant(const std::string& value)
{
    return g_variant_new_string(value.c_str());
}

static GVariant* value_to_variant(const std::vector<std::string>& value)
{
    std::vector<const gchar*> a;
    for (auto& v : value)
        a.emplace_back(v.c_str());

    return g_variant_new_strv(&a[0],
            static_cast<gssize>(a.size()));
}

template <typename TValue>
void read_vector(GSettings* settings,
                 const std::string& key_name,
                 std::vector<TValue>& value)
{
    value.clear();

    VariantPtr v(g_settings_get_value(settings, key_name.c_str()),
                 g_variant_unref);
    if (!v)
        throw Exception("read_vector: g_settings_get_value failed");
    gsize n = g_variant_n_children (v.get());
    for (gsize i=0; i<n; i++)
    {
        VariantPtr vval(g_variant_get_child_value (v.get(), i),
                        g_variant_unref);
        if (!vval)
            throw Exception("read_vector: g_variant_get_child_value failed");
        TValue tmp;
        variant_to_value(vval.get(), tmp);
        value.emplace_back(tmp);
    }
}

template <typename TValue>
void write_vector(GSettings* settings,
                 const std::string& key_name,
                 const std::vector<TValue>& value,
                 const std::string& type)
{
    GVariantBuilder builder;
    g_variant_builder_init (&builder, G_VARIANT_TYPE(type.c_str()));

    for (auto e : value)
    {
        GVariant* vval = value_to_variant(e);
        if (!vval)
            throw Exception("write_vector: value_to_variant failed for value " +
                            repr(e));
        g_variant_builder_add_value (&builder, vval);  // consumes floating reference
    }

    GVariant* v = g_variant_builder_end (&builder);
    if (!v)
        throw Exception("write_vector: g_variant_builder_end failed");

    // consumes floating reference of v
    g_settings_set_value(settings, key_name.c_str(), v);
}


template <typename TKey, typename TValue>
void read_map(GSettings* settings,
              const std::string& key_name,
              std::map<TKey, TValue>& value)
{
    VariantPtr v(g_settings_get_value(settings, key_name.c_str()),
                 g_variant_unref);
    if (!v)
        throw Exception("read_map: g_settings_get_value failed");

    gsize n = g_variant_n_children (v.get());
    for (gsize i=0; i<n; i++)
    {
        VariantPtr e(g_variant_get_child_value (v.get(), i),
                     g_variant_unref);
        if (!e)
            throw Exception("read_map: g_variant_get_child_value failed");

        if (g_variant_n_children (e.get()) != 2)
            throw Exception("read_map: g_variant_n_children != 2");

        VariantPtr vkey(g_variant_get_child_value (e.get(), 0),
                        g_variant_unref);
        if (!vkey)
            throw Exception("read_map: g_variant_get_child_value failed for key");
        TKey key;
        variant_to_value(vkey.get(), key);

        VariantPtr vval(g_variant_get_child_value (e.get(), 1),
                        g_variant_unref);
        if (!vval)
            throw Exception("read_map: g_variant_get_child_value failed for value");
        TValue val;
        variant_to_value(vval.get(), val);

        value[key] = val;
    }
}

template <typename TKey, typename TValue>
void write_map(GSettings* settings,
               const std::string& key_name,
               const std::map<TKey, TValue>& value,
               const std::string& type)
{
    GVariantBuilder builder;
    g_variant_builder_init (&builder, G_VARIANT_TYPE(type.c_str()));
    const GVariantType *element_type = g_variant_type_element (G_VARIANT_TYPE(type.c_str()));

    for (auto e : value)
    {
        g_variant_builder_open (&builder, element_type);

        GVariant* vkey = value_to_variant(e.first);
        if (!vkey)
            throw Exception("write_map: value_to_variant failed for key " +
                            repr(e.first));
        g_variant_builder_add_value (&builder, vkey);  // consumes floating reference

        GVariant* vval = value_to_variant(e.second);
        if (!vval)
            throw Exception("write_map: value_to_variant failed for value " +
                            repr(e.second));
        g_variant_builder_add_value (&builder, vval);  // consumes floating reference

        g_variant_builder_close (&builder);
    }

    GVariant* v = g_variant_builder_end (&builder);
    if (!v)
        throw Exception("write_map: g_variant_builder_end failed");

    // consumes floating reference of v
    g_settings_set_value(settings, key_name.c_str(), v);
}

void ConfigObject::read_value(const std::string& key_name, bool& value)
{
    value = g_settings_get_boolean(m_settings, key_name.c_str());
}
void ConfigObject::write_value(const std::string& key_name,
                               bool value,
                               const std::string& type_string)
{
    (void)type_string;
    g_settings_set_boolean(m_settings, key_name.c_str(), value);
}

void ConfigObject::read_value(const std::string& key_name, int& value)
{
    value = g_settings_get_int(m_settings, key_name.c_str());
}

void ConfigObject::write_value(const std::string& key_name,
                               int value,
                               const std::string& type_string)
{
    (void)type_string;
    g_settings_set_int(m_settings, key_name.c_str(), value);
}

void ConfigObject::read_value(const std::string& key_name, double& value)
{
    value = g_settings_get_double(m_settings, key_name.c_str());
}
void ConfigObject::write_value(const std::string& key_name,
                               double value,
                               const std::string& type_string)
{
    (void)type_string;
    g_settings_set_double(m_settings, key_name.c_str(), value);
}

void ConfigObject::read_value(const std::string& key_name, std::string& value)
{
    value = g_settings_get_string(m_settings, key_name.c_str());
}

void ConfigObject::write_value(const std::string& key_name,
                               const std::string& value,
                               const std::string& type_string)
{
    (void)type_string;
    g_settings_set_string(m_settings, key_name.c_str(), value.c_str());
}

int ConfigObject::read_enum(const std::string& key_name)
{
    return g_settings_get_enum(m_settings, key_name.c_str());
}
void ConfigObject::write_enum(const std::string& key_name, int value)
{
    g_settings_set_enum(m_settings, key_name.c_str(), value);
}

void ConfigObject::read_value(const std::string& key_name, std::vector<double>& value)
{
    read_vector(m_settings, key_name, value);
}
void ConfigObject::write_value(const std::string& key_name,
                               const std::vector<double>& value,
                               const std::string& type_string)
{
    write_vector(m_settings, key_name, value, type_string);
}

void ConfigObject::read_value(const std::string& key_name,
                              std::vector<std::string>& value)
{
    read_vector(m_settings, key_name, value);
}
void ConfigObject::write_value(const std::string& key_name,
                               const std::vector<std::string>& value,
                               const std::string& type_string)
{
    write_vector(m_settings, key_name, value, type_string);
}

void ConfigObject::read_value(const std::string& key_name,
                              std::vector<std::vector<std::string> >& value)
{
    read_vector(m_settings, key_name, value);
}

void ConfigObject::write_value(const std::string& key_name,
                               const std::vector<std::vector<std::string> >& value,
                               const std::string& type_string)
{
    write_vector(m_settings, key_name, value, type_string);
}


void ConfigObject::read_value(const std::string& key_name,
                              std::map<int, int>& value)
{
    read_map(m_settings, key_name, value);
}
void ConfigObject::write_value(const std::string& key_name,
                               const std::map<int, int>& value,
                               const std::string& type_string)
{
    write_map(m_settings, key_name, value, type_string);
}

void ConfigObject::read_value(const std::string& key_name,
                              std::map<std::string, std::string>& value)
{
    read_map(m_settings, key_name, value);
}
void ConfigObject::write_value(const std::string& key_name,
                               const std::map<std::string, std::string>& value,
                               const std::string& type_string)
{
    write_map(m_settings, key_name, value, type_string);
}


