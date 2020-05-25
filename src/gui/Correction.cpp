#include "Correction.h"

#include <config.h>

#include "control/Control.h"

#include "control/Control.h"
#include "gui/XournalView.h"
#include "gui/scroll/ScrollHandling.h"

#include "control/jobs/PdfExportJob.h"

#include "i18n.h"

/* FIXME: remove static variables */

static Paper *cur_paper = NULL;

static GtkWidget *close_button = NULL;

static void on_paper_clicked(GtkWidget *widget, gpointer data)
{
    Paper *paper = (Paper *)data;

    if (!paper->xopp.exists()) {
        /* create xopp */
        paper->corr->control->clearSelectionEndText();
        paper->corr->control->newFile();
        bool an = paper->corr->control->annotatePdf(paper->pdf.c_str(), false, false);
        if (!an) {
            return;
        }

        /* save xopp */
        Document *doc = paper->corr->control->getDocument();
        doc->lock();
        doc->setFilename(paper->xopp);
        doc->unlock();
        paper->corr->control->save(true);
    }
    else {
        /* open existing xopp */
        bool op = paper->corr->control->openFile(paper->xopp, -1, true);
        if (!op) {
            return;
        }
    }

    if (cur_paper) {
        GtkAdjustment *vertical = paper->corr->m_window->getXournal()->getScrollHandling()->getVertical();
        double value = gtk_adjustment_get_value(vertical);
        printf("value: %f\n", value);
    }

    auto itr = std::find(paper->corr->papers.begin(), paper->corr->papers.end(), paper);
    g_assert(itr != paper->corr->papers.cend());
    int index = std::distance(paper->corr->papers.begin(), itr);
    gtk_list_box_select_row(GTK_LIST_BOX(paper->corr->m_list),
            gtk_list_box_get_row_at_index(GTK_LIST_BOX(paper->corr->m_list), index));

    cur_paper = paper;
}

static void on_delete_clicked(GtkWidget *widget, gpointer data)
{
    Paper *paper = (Paper *)data;

    if (cur_paper == paper) {
        paper->corr->control->clearSelectionEndText();
        paper->corr->control->newFile();
        cur_paper = NULL;
    }

    gtk_container_remove(GTK_CONTAINER(paper->corr->m_list), gtk_widget_get_parent(paper->box));

    auto itr = std::find(paper->corr->papers.begin(), paper->corr->papers.end(), paper);
    g_assert(itr != paper->corr->papers.cend());
    paper->corr->papers.erase(itr);

    printf("papers:\n");
    for (auto p : paper->corr->papers) {
        printf("%s\n", p->xopp.getFilename().c_str());
    }
}

static gint compare(gconstpointer a, gconstpointer b)
{
    return strcmp((const char *)a, (const char *)b);
}

void Correction::onOpenList(void)
{
    GtkWidget *dialog;
    GtkFileChooser *chooser;
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
    GtkFileFilter *filter;
    gint res;

    dialog = gtk_file_chooser_dialog_new ("Open File", control->getGtkWindow(),
            action,
            _("_Cancel"),
            GTK_RESPONSE_CANCEL,
            _("_Open"),
            GTK_RESPONSE_ACCEPT,
            NULL);
    chooser = GTK_FILE_CHOOSER(dialog);

    filter = gtk_file_filter_new();
    gtk_file_filter_add_pattern(filter, "*.pdf");
    gtk_file_chooser_add_filter(chooser, filter);

    gtk_file_chooser_set_select_multiple(chooser, true);

    res = gtk_dialog_run(GTK_DIALOG(dialog));
    if (res == GTK_RESPONSE_ACCEPT) {
        char *filename;
        GSList *filenames = gtk_file_chooser_get_filenames(chooser);
        filenames = g_slist_sort(filenames, compare);
        for(GSList *f = filenames; f; f = f->next) {
            filename = (char *)f->data;
            printf("file: %s\n", filename);

            Paper *paper = new Paper;
            paper->corr = this;
            std::stringstream ss;
            ss << "file://" << filename;
            Path p(filename);
            paper->pdf = p;
            paper->xopp = paper->pdf;
            paper->xopp.clearExtensions(".pdf");
            paper->xopp += ".xopp";
            printf("xopp: %s\n", paper->xopp.c_str());
            if (!paper->xopp.exists()) {
                printf("xopp not found!\n");
            }
            papers.push_back(paper);

            /* create new line in side view */
            paper->box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
            GtkWidget *button = gtk_button_new_with_label(paper->pdf.getFilename().c_str());
            g_signal_connect(button, "clicked", G_CALLBACK(on_paper_clicked), paper);
            GtkWidget *del_button = gtk_button_new_from_icon_name("edit-delete", GTK_ICON_SIZE_BUTTON);
            g_signal_connect(del_button, "clicked", G_CALLBACK(on_delete_clicked), paper);
            gtk_box_pack_start(GTK_BOX(paper->box), button, TRUE, TRUE, 2);
            gtk_box_pack_end(GTK_BOX(paper->box), del_button, FALSE, FALSE, 2);
            gtk_widget_show_all(paper->box);
            gtk_list_box_insert(GTK_LIST_BOX(m_list), paper->box, -1);
        }
    }

    gtk_widget_destroy(dialog);
}

static gboolean export_next(gpointer data)
{
    Correction *corr = (Correction *)data;

    g_assert(corr->export_index < corr->papers.size());

    Paper *paper = corr->papers[corr->export_index];

    printf("paper: %s\n", paper->pdf.c_str());

    corr->control->openFile(paper->xopp, -1, true);

    std::stringstream ss;
    std::filesystem::path *corrected = new std::filesystem::path(paper->pdf.str());
    std::string name = corrected->stem().c_str();
    name = name + "-Corrected.pdf";
    corrected->replace_filename(name);
    ss << "file://" << corrected->c_str();
    printf("corrected: %s\n", ss.str().c_str());
    Path path = Path::fromUri(ss.str());

    corr->control->clearSelectionEndText();
    PdfExportJob *job = new PdfExportJob(corr->control);
    job->setFilename(path);
    corr->control->getScheduler()->addJob(job, JOB_PRIORITY_NONE);

    job->unref();

    corr->export_index++;

    GtkWidget *vbox = gtk_dialog_get_content_area(GTK_DIALOG(corr->m_dialog));

    GtkWidget *label = gtk_label_new (corrected->c_str());
    gtk_container_add(GTK_CONTAINER(vbox), label);
    gtk_widget_show(label);

    if (corr->export_index >= corr->papers.size()) {
        gtk_container_remove(GTK_CONTAINER(vbox), corr->m_spinner);

        GtkWidget *label = gtk_label_new ("Done!");
        gtk_container_add(GTK_CONTAINER(vbox), label);
        gtk_widget_show(label);

        gtk_widget_set_sensitive(close_button, TRUE);

        return FALSE;
    }

    return TRUE;
}

void Correction::onGenerate(void)
{
    if(papers.size() == 0) {
        /* TODO: show error dialog */
        return;
    }

    // FIXME: temporarily close sidebar (sidebar repaint causes SIGSEGV)
    m_window->setSidebarVisible(false);

    m_dialog = gtk_dialog_new_with_buttons ("Message",
            control->getGtkWindow(),
            GTK_DIALOG_MODAL,
            NULL);
    g_signal_connect_swapped (m_dialog,
            "response",
            G_CALLBACK (gtk_widget_destroy),
            m_dialog);

    GtkWidget *vbox = gtk_dialog_get_content_area(GTK_DIALOG(m_dialog));

    GtkWidget *label = gtk_label_new ("Exporting PDFs");
    gtk_container_add(GTK_CONTAINER(vbox), label);

    m_spinner = gtk_spinner_new();
    gtk_container_add(GTK_CONTAINER(vbox), m_spinner);
    gtk_spinner_start(GTK_SPINNER(m_spinner));

    gtk_widget_set_size_request(vbox, 640, 480);

    close_button = gtk_dialog_add_button(GTK_DIALOG(m_dialog), "Close", GTK_RESPONSE_ACCEPT);
    gtk_widget_set_sensitive(close_button, FALSE);

    gtk_widget_show_all(m_dialog);

    export_index = 0;
    g_idle_add(export_next, this);
}

Correction::Correction(Control* control) : control(control)
{
    m_window = control->getWindow();

    m_open = m_window->get("btCorrectionOpenList");
    g_signal_connect(m_open, "clicked", G_CALLBACK(
                +[](GtkButton* button, Correction* self) { self->onOpenList(); }
                ), this);

    m_generate = m_window->get("btCorrectionGenerate");
    g_signal_connect(m_generate, "clicked", G_CALLBACK(
                +[](GtkButton* button, Correction* self) { self->onGenerate(); }
                ), this);

    m_list = m_window->get("listCorrection");
}

Correction::~Correction()
{
    this->control = nullptr;
}
