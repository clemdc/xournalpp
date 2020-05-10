#include "Correction.h"

#include <config.h>

#include "control/Control.h"

#include "control/Control.h"
#include "gui/XournalView.h"

#include "control/jobs/PdfExportJob.h"

#include "i18n.h"

static gboolean YourCallBack(void* data)
{
    Correction *corr = (Correction *)data; 
    corr->control->getZoomControl()->zoom100();
    return FALSE;
}

#if 0
void Correction::onItemClicked(GtkWidget *widget)
{
    const gchar *filename = gtk_button_get_label(GTK_BUTTON(widget));
    printf("Clicked: %s\n", filename);

    // TODO: if xopp not found, create it and annotate the pdf
    //       else xopp found, open it
    control->clearSelectionEndText();
    control->newFile();
    control->annotatePdf(filename, false, false);


#if 0
    printf("START ZOOMING\n");
    m_window->getXournal()->zoomIn();
    m_window->getXournal()->zoomOut();
    printf("DONE ZOOMING\n");
#endif

    g_timeout_add_seconds(1, YourCallBack, this);

    //m_window->getXournal()->zoomChanged();
    //control->fireActionSelected(GROUP_ZOOM_FIT, ACTION_ZOOM_100);
    //control->getZoomControl()->zoom100();


    // TODO: unhighlight current
    //       highlight next
}
#endif

static void on_paper_clicked(GtkWidget *widget, gpointer data)
{
    Paper *paper = (Paper *)data; 

    if (!std::filesystem::exists(*paper->xopp)) {
        /* create xopp */
        paper->corr->control->clearSelectionEndText();
        paper->corr->control->newFile();
        paper->corr->control->annotatePdf(paper->pdf->c_str(), false, false);
        
        /* save xopp */
        std::stringstream ss;
        ss << "file://" << paper->xopp->c_str();
        Path path = Path::fromUri(ss.str());
        Document *doc = paper->corr->control->getDocument();
        doc->lock();
        doc->setFilename(path);
        doc->unlock();
        paper->corr->control->save(true);
    }
    else {
        /* open existing xopp */
        std::stringstream ss;
        ss << "file://" << paper->xopp->c_str();
        Path path = Path::fromUri(ss.str());
        paper->corr->control->openFile(path, -1, true);
    }

    g_timeout_add_seconds(1, YourCallBack, paper->corr);
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
        for(GSList *f = filenames; f; f = f->next) {
            filename = (char *)f->data;
            printf("file: %s\n", filename);

            Paper *paper = new Paper;
            paper->corr = this;
            paper->pdf = new std::filesystem::path(filename);
            paper->xopp = new std::filesystem::path(filename);
            paper->xopp->replace_extension(".xopp");
            if (!std::filesystem::exists(*paper->xopp)) {
                printf("xopp not found!\n");
            }
            papers.push_back(paper);

            /* create new line in side view */
            GtkWidget *button = gtk_button_new_with_label(paper->pdf->filename().c_str());
            g_signal_connect(button, "clicked", G_CALLBACK(on_paper_clicked), paper);
            gtk_list_box_insert(GTK_LIST_BOX(m_list), button, -1);
            gtk_widget_show(button);
        }
    }

    gtk_widget_destroy(dialog);
}

GtkWidget *close_button = NULL;

static gboolean export_next(gpointer data)
{
    Correction *corr = (Correction *)data;
    Paper *paper = corr->papers[corr->export_index];

    printf("paper: %s\n", paper->pdf->c_str());

    {
        std::stringstream ss;
        ss << "file://" << paper->xopp->c_str();
        Path path = Path::fromUri(ss.str());
        corr->control->openFile(path, -1, true);
    }

    std::stringstream ss;
    std::filesystem::path *corrected = new std::filesystem::path(*paper->pdf);
    std::string name = corrected->stem().c_str();
    name = name + "-Corrected.pdf";
    corrected->replace_filename(name);
    ss << "file://" << corrected->c_str();
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
    printf("Ma Chérie <3\n");

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

    printf("DONE\n");

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
