#ifndef __ELSA_WM_PLUGIN_H__
#define __ELSA_WM_PLUGIN_H__

#include <meta/meta-plugin.h>

#define ELSA_TYPE_WM_PLUGIN            (elsa_wm_plugin_get_type())
#define ELSA_WM_PLUGIN(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), ELSA_TYPE_WM_PLUGIN, ElsaWmPlugin))
#define ELSA_WM_PLUGIN_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  ELSA_TYPE_WM_PLUGIN, ElsaWmPluginClass))
#define ELSA_IS_WM_PLUGIN(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), ELSA_TYPE_WM_PLUGIN))
#define ELSA_IS_WM_PLUGIN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  ELSA_TYPE_WM_PLUGIN))
#define ELSA_WM_PLUGIN_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  ELSA_TYPE_WM_PLUGIN, ElsaWmPluginClass))

#define ELSA_WM_PLUGIN_GET_PRIVATE(obj) \
(G_TYPE_INSTANCE_GET_PRIVATE ((obj), ELSA_TYPE_WM_PLUGIN, ElsaWmPluginPrivate))

typedef struct _ElsaWmPlugin        ElsaWmPlugin;
typedef struct _ElsaWmPluginClass   ElsaWmPluginClass;
typedef struct _ElsaWmPluginPrivate ElsaWmPluginPrivate;

struct _ElsaWmPlugin
{
    MetaPlugin parent;

    ElsaWmPluginPrivate *priv;
};

struct _ElsaWmPluginClass
{
    MetaPluginClass parent_class;
};

GType elsa_wm_plugin_get_type (void) G_GNUC_CONST;
ElsaWmPlugin *elsa_wm_plugin_get();
MetaScreen *elsa_wm_plugin_get_screen(ElsaWmPlugin *elsa_wm_plugin);
MetaDisplay *elsa_wm_plugin_get_display(ElsaWmPlugin *elsa_wm_plugin);
guint elsa_wm_get_action(ElsaWmPlugin *elsa_wm_plugin, const char *name);

#endif /* __ELSA_WM_PLUGIN_H__ */
