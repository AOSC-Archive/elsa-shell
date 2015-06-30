/* Copyright (C) 2014 - 2015 Sian Cao <siyuan.cao@i-soft.com.cn> */

#ifndef __META_SWITCHER_H__
#define __META_SWITCHER_H__

#include <glib-object.h>
#include <meta/meta-plugin.h>
#include <meta/keybindings.h>
#include <clutter/clutter.h>

#define META_TYPE_SWITCHER            (meta_switcher_get_type())
#define META_SWITCHER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), META_TYPE_SWITCHER, MetaSwitcher))
#define META_SWITCHER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  META_TYPE_SWITCHER, MetaSwitcherClass))
#define META_IS_SWITCHER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), META_TYPE_SWITCHER))
#define META_IS_SWITCHER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  META_TYPE_SWITCHER))
#define META_SWITCHER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  META_TYPE_SWITCHER, MetaSwitcherClass))
#define META_SWITCHER_ERROR           meta_switcher_error_quark()

typedef struct _MetaSwitcher MetaSwitcher;
typedef struct _MetaSwitcherClass MetaSwitcherClass;
typedef struct _MetaSwitcherPrivate MetaSwitcherPrivate;

struct _MetaSwitcher
{
  GObject parent;
  MetaSwitcherPrivate* priv;
};

struct _MetaSwitcherClass
{
  GObjectClass parent_class;
  void (*destroy)(MetaSwitcher*);
};

typedef enum
{
  META_SWITCHER_ERROR_OTHER
} MetaSwitcherError;

G_BEGIN_DECLS

GType                   meta_switcher_get_type          (void) G_GNUC_CONST;
gint                    meta_switcher_error_quark       (void) G_GNUC_CONST;

MetaSwitcher* meta_switcher_new(MetaPlugin*);
gboolean meta_switcher_show(MetaSwitcher* self);

G_END_DECLS

#endif /* __META_SWITCHER_H__ */
