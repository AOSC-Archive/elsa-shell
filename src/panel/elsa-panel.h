/* Copyright (C) 2015 Leslie Zhai <xiang.zhai@i-soft.com.cn> */

#ifndef __ELSA_PANEL_H__
#define __ELSA_PANEL_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define ELSA_TYPE_PANEL            (elsa_panel_get_type())           
#define ELSA_PANEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), ELSA_TYPE_PANEL, ElsaPanel))
#define ELSA_PANEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), ELSA_TYPE_PANEL, ElsaPanelClass))
#define ELSA_IS_PANEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), ELSA_TYPE_PANEL))
#define ELSA_IS_PANEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), ELSA_TYPE_PANEL))
#define ELSA_PANEL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), ELSA_TYPE_PANEL, ElsaPanelClass))
                                                                                
typedef struct _ElsaPanel           ElsaPanel;                          
typedef struct _ElsaPanelClass      ElsaPanelClass;                     
typedef struct _ElsaPanelPrivate    ElsaPanelPrivate;

struct _ElsaPanel {
    GObject parent;
    
    ElsaPanelPrivate *priv;
};

struct _ElsaPanelClass {
    GObjectClass parent_class;

    /* for future expansion */
    void (*_gtk_reserved1) (void);
};

GDK_AVAILABLE_IN_ALL
GType elsa_panel_get_type (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_ALL
ElsaPanel *elsa_panel_new();

GDK_AVAILABLE_IN_ALL
GdkScreen *elsa_panel_get_screen(ElsaPanel *self);

GDK_AVAILABLE_IN_ALL
gint elsa_panel_get_height(ElsaPanel *self);

GDK_AVAILABLE_IN_ALL
void elsa_panel_show(ElsaPanel *self);

G_END_DECLS

#endif /* __ELSA_PANEL_H__ */
