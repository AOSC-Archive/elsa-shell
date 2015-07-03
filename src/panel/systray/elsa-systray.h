/* Copyright (C) 2015 AnthonOS Open Source Community */

#ifndef __ELSA_SYSTRAY_H__
#define __ELSA_SYSTRAY_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define ELSA_TYPE_SYSTRAY             (elsa_systray_get_type())
#define ELSA_SYSTRAY(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), ELSA_TYPE_SYSTRAY, ElsaSystray))
#define ELSA_SYSTRAY_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), ELSA_TYPE_SYSTRAY, ElsaSystrayClass))
#define ELSA_IS_SYSTRAY(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), ELSA_TYPE_SYSTRAY))
#define ELSA_IS_SYSTRAY_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), ELSA_TYPE_SYSTRAY))
#define ELSA_SYSTRAY_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), ELSA_TYPE_SYSTRAY, ElsaSystrayClass))

typedef struct _ElsaSystray         ElsaSystray;
typedef struct _ElsaSystrayClass    ElsaSystrayClass;
typedef struct _ElsaSystrayPrivate  ElsaSystrayPrivate;

struct _ElsaSystray
{
    GtkBin parent;

    ElsaSystrayPrivate *priv;
};

struct _ElsaSystrayClass
{
    GtkBinClass parent_class;

    /* Padding for future expansion */
    void (*_gtk_reserved1) (void);
};

GType      elsa_systray_get_type    (void) G_GNUC_CONST;
GtkWidget *elsa_systray_new         (GdkScreen *screen);

G_END_DECLS

#endif /* __ELSA_SYSTRAY_H__ */
