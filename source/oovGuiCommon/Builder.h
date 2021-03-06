/*
* Builder.h
*
*  Created on: Jun 19, 2013
*  \copyright 2013 DCBlaha.  Distributed under the GPL.
*/

#ifndef BUILDER_H_
#define BUILDER_H_

#include <gtk/gtk.h>
#include "OovString.h"


/// This wraps the GtkBuilder.
/// This loads a GTK Glade file that defines user interface components.
class Builder
    {
    public:
        Builder();
        ~Builder()
            {
            g_object_unref(mGtkBuilder);
            }
        bool addFromFile(OovStringRef const fn);
        void connectSignals()
            {
            gtk_builder_connect_signals(mGtkBuilder, nullptr);
            }
        GObject *getObject(OovStringRef const widgetName)
            {
            return gtk_builder_get_object(mGtkBuilder, widgetName);
            }
        GtkWidget *getWidget(OovStringRef const widgetName)
            {
            return GTK_WIDGET(getObject(widgetName));
            }
        GtkMenu *getMenu(OovStringRef const menuName)
            {
            return GTK_MENU(gtk_builder_get_object(mGtkBuilder, menuName));
            }
        static Builder *getBuilder();
    private:
        GtkBuilder *mGtkBuilder;
    };



#endif /* BUILDER_H_ */
