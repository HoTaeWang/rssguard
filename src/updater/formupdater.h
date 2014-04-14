#ifndef FORMUPDATER_H
#define FORMUPDATER_H

#include <QMainWindow>

#include <QHash>


class QTextEdit;
class QKeyEvent;

class FormUpdater : public QMainWindow {
    Q_OBJECT

  public:
    enum UpdaterState {
      NoState,
      ExitNormal,
      ExitError
    };

    // Constructors and destructors.
    explicit FormUpdater(QWidget *parent = 0);
    virtual ~FormUpdater();

    void startUpgrade();
    void saveArguments();
    void printArguments();
    bool printUpdateInformation();
    bool doPreparationCleanup();
    bool doExtractionAndCopying();
    bool doFinalCleanup();

  protected:
    void keyPressEvent(QKeyEvent *event);

  private:
    void printText(const QString &text);
    void printNewline();
    void printHeading(const QString &header);
    void moveToCenterAndResize();

    // File/directory manipulators.
    bool copyDirectory(QString source, QString destination);
    bool removeDirectory(const QString & directory_name,
                         const QStringList &exception_file_list = QStringList(),
                         const QStringList &exception_folder_list = QStringList());

  private:
    UpdaterState m_state;
    QTextEdit *m_txtOutput;

    QHash<QString, QString> m_parsedArguments;
};

#endif // FORMUPDATER_H