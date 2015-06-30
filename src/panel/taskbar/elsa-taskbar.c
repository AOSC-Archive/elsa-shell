/* Copyright (C) 2015 Leslie Zhai <xiang.zhai@i-soft.com.cn> */

#include "config.h"
#include "elsa-taskbar.h"

#define WNCK_I_KNOW_THIS_IS_UNSTABLE
#include <libwnck/libwnck.h>

static GtkWidget *tasklist = NULL;

GtkWidget *elsa_taskbar_new() 
{
    tasklist = wnck_tasklist_new();

    wnck_tasklist_set_grouping(WNCK_TASKLIST(tasklist), 
        WNCK_TASKLIST_ALWAYS_GROUP);

    return tasklist;
}

void elsa_taskbar_set_width(int width) 
{
    wnck_tasklist_set_width(WNCK_TASKLIST(tasklist), width);
}
