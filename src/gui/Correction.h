#pragma once

#include <string>
#include <vector>
#include <filesystem>

#include <gtk/gtk.h>

#include "XournalType.h"

class Control;
class MainWindow;
class Paper;

class Correction {
public:
    Correction(Control* control);
    virtual ~Correction();

public:
    Control* control;

    MainWindow* m_window;
    GtkWidget* m_open;
    GtkWidget* m_generate;
    GtkWidget* m_list;

    GtkWidget* m_dialog;
    GtkWidget* m_close;
    GtkWidget* m_spinner;

    int export_index;
    std::vector<Paper *> papers;

    void onOpenList(void);
    void onGenerate(void);
};

class Paper {
public:
    Correction *corr;
    std::filesystem::path *pdf;
    std::filesystem::path *xopp;
    GtkWidget *box;
};
