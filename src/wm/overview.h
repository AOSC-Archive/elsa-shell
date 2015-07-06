/* Copyrgiht (C) 2014 - 2015 Sian Cao <siyuan.cao@i-soft.com.cn> */

#ifndef __MOSES_OVERVIEW_H__
#define __MOSES_OVERVIEW_H__

#include <glib-object.h>
#include <meta/meta-plugin.h>
#include <clutter/clutter.h>

#define MOSES_TYPE_OVERVIEW            (moses_overview_get_type())
#define MOSES_OVERVIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), MOSES_TYPE_OVERVIEW, MosesOverview))
#define MOSES_OVERVIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  MOSES_TYPE_OVERVIEW, MosesOverviewClass))
#define MOSES_IS_OVERVIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), MOSES_TYPE_OVERVIEW))
#define MOSES_IS_OVERVIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  MOSES_TYPE_OVERVIEW))
#define MOSES_OVERVIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  MOSES_TYPE_OVERVIEW, MosesOverviewClass))
#define MOSES_OVERVIEW_ERROR           moses_overview_error_quark()

typedef struct _MosesOverview MosesOverview;
typedef struct _MosesOverviewClass MosesOverviewClass;
typedef struct _MosesOverviewPrivate MosesOverviewPrivate;

struct _MosesOverview
{
    ClutterActor parent;
    MosesOverviewPrivate* priv;
};

struct _MosesOverviewClass
{
    ClutterActorClass parent_class;
};

typedef enum
{
    MOSES_OVERVIEW_ERROR_OTHER
} MosesOverviewError;

typedef enum
{
    MOSES_OV_REASON_NORMAL,
    MOSES_OV_REASON_ACTIVATE_WORKSPACE,
    MOSES_OV_REASON_ACTIVATE_WINDOW,

} MosesOverviewQuitReason;

G_BEGIN_DECLS

GType                   moses_overview_get_type         (void) G_GNUC_CONST;
gint                    moses_overview_error_quark      (void) G_GNUC_CONST;

MosesOverview*          moses_overview_new              (MetaPlugin*);
void moses_overview_show(MosesOverview*, gboolean all_windows);
MetaPlugin* overview_get_plugin(MosesOverview*);

G_END_DECLS

#endif
