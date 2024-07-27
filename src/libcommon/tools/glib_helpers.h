#ifndef SMART_POINTERS_H
#define SMART_POINTERS_H

#include <memory>

#include <glib-object.h>

typedef std::unique_ptr<GArray, void(*)(GArray*)> GArrayPtr;
typedef std::unique_ptr<gchar, decltype(&g_free)> GStrPtr;
typedef std::unique_ptr<GHashTable, decltype(&g_hash_table_destroy)> GHashTablePtr;
typedef std::unique_ptr<GQueue, decltype(&g_queue_free)> GQueuePtr;
typedef std::unique_ptr<GVariant, decltype(&g_variant_unref)> VariantPtr;

#endif // SMART_POINTERS_H
