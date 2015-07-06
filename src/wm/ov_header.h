/* Copyrgiht (C) 2014 - 2015 Sian Cao <siyuan.cao@i-soft.com.cn> */

#ifndef __OVERVIEW_HEAD_H__
#define __OVERVIEW_HEAD_H__

#include <glib-object.h>
#include <clutter/clutter.h>
#include <meta/types.h>

#define OVERVIEW_TYPE_HEAD            (overview_head_get_type())
#define OVERVIEW_HEAD(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), OVERVIEW_TYPE_HEAD, OverviewHead))
#define OVERVIEW_HEAD_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  OVERVIEW_TYPE_HEAD, OverviewHeadClass))
#define OVERVIEW_IS_HEAD(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), OVERVIEW_TYPE_HEAD))
#define OVERVIEW_IS_HEAD_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  OVERVIEW_TYPE_HEAD))
#define OVERVIEW_HEAD_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  OVERVIEW_TYPE_HEAD, OverviewHeadClass))
#define OVERVIEW_HEAD_ERROR           overview_head_error_quark()

typedef struct _OverviewHead OverviewHead;
typedef struct _OverviewHeadClass OverviewHeadClass;
typedef struct _OverviewHeadPrivate OverviewHeadPrivate;

struct _OverviewHead
{
  GObject parent;
  OverviewHeadPrivate* priv;
};

struct _OverviewHeadClass
{
  GObjectClass parent_class;
};

typedef enum
{
  OVERVIEW_HEAD_ERROR_OTHER
} OverviewHeadError;

G_BEGIN_DECLS

GType                   overview_head_get_type          (void) G_GNUC_CONST;
GQuark                  overview_head_error_quark       (void) G_GNUC_CONST;

typedef struct _MosesOverview MosesOverview;
OverviewHead*           overview_head_new               (MosesOverview*);

ClutterActor* overview_head_get_content(OverviewHead*);
ClutterActor* overview_head_get_actor_for_workspace(OverviewHead*, MetaWorkspace*);

G_END_DECLS

#endif
