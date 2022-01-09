#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include "QProcess"
#include <QMainWindow>
#include "QCheckBox"
#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QWidget>
#include <QCloseEvent>
#include <QComboBox>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE


struct Package
{
    std::string name;
    std::string current_version;
    std::string latest_version;
    QCheckBox* state;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    static const QString PACMAN_BINARY_PATH;
    static const QString CHECKUPDATES_BINARY_PATH;
    QScrollBar *consoleScollbar;
    QCheckBox *bulkSelectInput;
    QPushButton *refreshPackagesButton;
    QPushButton *updatePackagesButton;
    QComboBox * selectProgramDropdown;
    QPlainTextEdit *consoleOutput;
    int selectedPackages;
    std::vector<Package> packages;
    QProcess * process;
    std::vector<QCheckBox*> checkboxes;
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void on_bulkSelectInputPressed(int checkedState);
    void on_PackageSelect(int state);
    void showSystemTrayIcon();
    void initUi();
    void lockFileIsPresent();
    void checkDependencies();
    int showPopup(int level, QString title, QString description, bool exitFailure = false);
    void fetchUpdates();
    void processCheckUpdates();
    void processOutput();
    void updateRefreshButtonCount();
    void resetTable();
    void initProcess();
    bool hasTimeshift();
    bool handleTimeshift();
    void enableTimeshiftHook();
    void disableTimeshiftHook();
    bool doAdminActions(QStringList);
private:
    Ui::MainWindow *ui;
    bool closing;
    void isAlreadyRunning();
private slots:
    void on_updatePackagesButtonClicked();
    void on_refreshPackagesButtonClicked();
    void on_readyReadStandardOutput();
    void on_finished(int exitCode, QProcess::ExitStatus exitStatus);
};
#endif // MAINWINDOW_H
