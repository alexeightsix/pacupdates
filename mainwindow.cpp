#include "iostream"
#include <regex>
#include "QDebug"
#include "QFileInfo"
#include "QList"
#include "QProcess"
#include "QScrollBar"
#include "QTableWidgetItem"
#include <QAction>
#include <QCloseEvent>
#include <QMenu>
#include <QMessageBox>
#include <QMessageBox>
#include <QRect>
#include <QInputDialog>
#include <QSystemTrayIcon>
#include "./ui_mainwindow.h"
#include "mainwindow.h"
#include "QDir"
#include <unistd.h>
#include <iostream>
#include <QComboBox>
#include <QMetaObject>
#include <QMetaProperty>
#include <QVariant>

// - FIX TIMESHIFT
// - CLEAR TERM ON CANCEL BUTTON
// - SCROLL TO BOTTOM ON INPUT
// - ADD SOME LOADING ANIMATIONS
// - CLEANUP CODE

MainWindow::MainWindow(QWidget * parent): QMainWindow(parent), ui(new Ui::MainWindow)
{
    if (getuid() == 0) {
        int res = showPopup(QMessageBox::Warning, "Application is running in elevated permissions.", "Application is running in elevated permissions. Avoid running as root/sudo.", false);
    }

    ui->setupUi(this);

    MainWindow::initProcess();
    MainWindow::lockFileIsPresent();
    MainWindow::checkDependencies();
    MainWindow::initUi();
    MainWindow::fetchUpdates();
    MainWindow::showSystemTrayIcon();
}

bool MainWindow::doAdminActions(QStringList actions)
{
    bool ok;
    QString password = QInputDialog::getText(this, tr("This task requires administrator privileges"), tr("Password"), QLineEdit::Password, "", &ok);
    int exitCode = 0;

    if (ok){
        for(int i = 0; i < actions.size(); i++) {
            if (exitCode != 0)
                return exitCode;
            exitCode = system("echo " + password.toUtf8() + " | sudo -u root -S "+QString(actions[i]).toUtf8());
        }
    }
    return exitCode;
}

bool MainWindow::handleTimeshift()
{
    if (!MainWindow::hasTimeshift())
        return 0;

    int response = showPopup(QMessageBox::Question, "Timeshift Autosnap", "Timeshift has been detected on your system. Do you want to disable autosnaps?");

    int hookDisabled = QFile::exists("/etc/pacman.d/hooks/01-timeshift-autosnap.hook");

    int exitCode = 0;

    if ((response == QMessageBox::Cancel || response == QMessageBox::No) && hookDisabled) {
        exitCode = MainWindow::doAdminActions({"rm /etc/pacman.d/hooks/01-timeshift-autosnap.hook"});
    }

    if(response == QMessageBox::Yes && !hookDisabled) {
        exitCode = MainWindow::doAdminActions({"mkdir -p /etc/pacman.d/hooks/", "touch /etc/pacman.d/hooks/01-timeshift-autosnap.hook"});
    }

    return exitCode;
}

void MainWindow::enableTimeshiftHook()
{
    if(QFile::exists("/etc/pacman.d/hooks/01-timeshift-autosnap.hook")) {
        int response = showPopup(QMessageBox::Question, "Elevated Permissions Required", "Elevated permissions required to delete /etc/pacman.d/hooks/01-timeshift-autosnap.hook");

        if (response == QMessageBox::Cancel || response == QMessageBox::No) {

        } else if (response == QMessageBox::Yes) {
             bool ok;
             QString password = QInputDialog::getText(this, tr("This task requires administrator privileges"), tr("Password"), QLineEdit::Password, "", &ok);

             if (ok){
                 system("echo "+password.toUtf8()+"| sudo -u rm /etc/pacman.d/hooks/01-timeshift-autosnap.hook");
             }
        }
    }
}

void MainWindow::initProcess()
{
    process = new QProcess(this);

    process->setReadChannel(QProcess::StandardOutput);
    process->setProcessChannelMode(QProcess::MergedChannels);
    process->setCurrentReadChannel(QProcess::StandardOutput);
    process->setProperty("context", "");

    connect(process, SIGNAL(readyReadStandardOutput()), this, SLOT(on_readyReadStandardOutput()));
    connect(process, static_cast<void(QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished), [=](int exitCode, QProcess::ExitStatus exitStatus){
        MainWindow::on_finished(exitCode, exitStatus);
    });
}

void MainWindow::showSystemTrayIcon()
{
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        QSystemTrayIcon *systemTrayIcon = new QSystemTrayIcon;
        systemTrayIcon->setIcon(QIcon(":/ico.png"));
        systemTrayIcon->setVisible(true);
        systemTrayIcon->show();
    }
}

void MainWindow::on_finished(int exitCode, QProcess::ExitStatus exitStatus)
{

    QString context = process->property("context").toString();

    qDebug() << exitCode;
    qDebug() <<  exitStatus;

    process->setProperty("context", "");

    if (context == "cancelled") {
        qDebug() << "asdasd?";
        process->close();
        on_refreshPackagesButtonClicked();
        return;
    }

    if (exitStatus == QProcess::ExitStatus::CrashExit || exitCode == QProcess::ExitStatus::CrashExit ) {
        showPopup(QMessageBox::Critical, "Crashed Exit", "Update crashed.", true);
        return;
    }

    bool isUpdating = process->arguments().contains("-S");
    bool isCheckingForUpdates = process->arguments().contains("-Qu") || process->program() == "/usr/bin/checkupdates";

    if (isUpdating) {
        on_refreshPackagesButtonClicked();
        showPopup(QMessageBox::Information, "Updates Complete!", "The updates to your selected packages have been successfully updated!");
    }

    if (isCheckingForUpdates) {
        MainWindow::updateRefreshButtonCount();
        refreshPackagesButton->setEnabled(true);
        selectProgramDropdown->setEnabled(true);
    }
}

int MainWindow::showPopup(int level, QString title, QString description = "", bool exitFailure)
{
    QMessageBox icon;
    QMessageBox msgBox;

    msgBox.setWindowTitle(title);

    if (description != "") {
        msgBox.setText(description);
    }

    if (level == QMessageBox::Critical) {
        msgBox.setIcon(QMessageBox::Critical);
    }

    if (level == QMessageBox::Question) {
        msgBox.setIcon(QMessageBox::Question);
        msgBox.setStandardButtons(QMessageBox::Cancel | QMessageBox::Yes | QMessageBox::No);
        msgBox.setDefaultButton(QMessageBox::Cancel);
    }

   if (level == QMessageBox::Warning) {
       msgBox.setIcon(QMessageBox::Warning);
       msgBox.setStandardButtons(QMessageBox::Ok);
   }

    if (level == QMessageBox::Critical) {
        msgBox.exec();
        process->close();
        if (exitFailure)
            exit(EXIT_FAILURE);
    }

    return msgBox.exec();
}

void MainWindow::lockFileIsPresent()
{
    QString lockFilePath = "/var/lib/pacman/db.lck";
    QFileInfo db_lck(lockFilePath);
    if (db_lck.exists())
        MainWindow::showPopup(QMessageBox::Critical, "Lockfile Is Present", "Database lockfile is present. Delete /var/lib/pacman/db.lck or close application that is using it.");
}

void MainWindow::initUi()
{
    // Set Default MainWindow Dimensions
    const int MainWindowWidth = 735;
    QSize WindowSize = QSize(MainWindowWidth, 750);
    QMainWindow::resize(WindowSize);
    QMainWindow::setMinimumSize(WindowSize);
    QMainWindow::setMaximumSize(WindowSize);
    QMainWindow::setWindowTitle(QString("Pacman Simple Package Updater"));

    // Set packagesTable Dimensions
    ui->packagesTable->setMinimumWidth(MainWindowWidth);
    ui->packagesTable->setMinimumHeight(650);
    ui->packagesTable->setEnabled(true);
    ui->packagesTable->setVisible(true);

    selectProgramDropdown = new QComboBox(ui->centralwidget);
    selectProgramDropdown->setObjectName("selectProgramDropdown");
    selectProgramDropdown->setGeometry(QRect(15, 680, 171, 31));

    QStringList listOfPrograms = {"pacman"};

    if(system("which yay >> /dev/null 2>>/dev/null") == 0) {
        listOfPrograms.push_front("yay");
    }

    selectProgramDropdown->addItems(listOfPrograms);

    // Refresh Button
    refreshPackagesButton = new QPushButton(ui->centralwidget);
    refreshPackagesButton->setObjectName("refreshPackagesButton");
    refreshPackagesButton->setGeometry(QRect(350, 680, 171, 31));
    refreshPackagesButton->setText("Refresh Packages");
    refreshPackagesButton->setEnabled(false);

    // Update Button
    updatePackagesButton = new QPushButton(ui->centralwidget);
    updatePackagesButton->setObjectName("updatePackagesButton");
    updatePackagesButton->setGeometry(QRect(540, 680, 171, 31));
    updatePackagesButton->setText("Update Packages (0)");
    updatePackagesButton->setEnabled(false);

    // Bulk Select Checkbox
    bulkSelectInput = new QCheckBox(ui->centralwidget);
    bulkSelectInput->setObjectName("bulkSelectInput");
    bulkSelectInput->setGeometry(QRect(585, 3, 16, 16));
    bulkSelectInput->setChecked(true);
    bulkSelectInput->setEnabled(true);

    // Scrollbar
    QScrollBar * consoleScollbar = new QScrollBar();

    // Console Output
    consoleOutput = new QPlainTextEdit(ui->centralwidget);
    consoleOutput->setObjectName("consoleOutput");
    consoleOutput->setEnabled(false);
    consoleOutput->setMinimumWidth(MainWindowWidth);
    consoleOutput->setMinimumHeight(650);
    consoleOutput->setEnabled(true);
    consoleOutput->setReadOnly(true);
    consoleOutput->setVerticalScrollBar(consoleScollbar);

    consoleOutput->hide();

    connect(bulkSelectInput, &QCheckBox::stateChanged, this, &MainWindow::on_bulkSelectInputPressed);
    connect(refreshPackagesButton, SIGNAL(clicked()), this, SLOT(on_refreshPackagesButtonClicked()));
    connect(updatePackagesButton, SIGNAL(clicked()), this, SLOT(on_updatePackagesButtonClicked()));
}

void MainWindow::on_refreshPackagesButtonClicked()
{
    consoleOutput->hide();
    ui->packagesTable->show();

    refreshPackagesButton->setEnabled(false);
    updatePackagesButton->setEnabled(false);
    selectProgramDropdown->setEnabled(false);
    resetTable();
    fetchUpdates();
}

void MainWindow::on_updatePackagesButtonClicked()
{
    int exitCode = 1;

   // do {
    //    exitCode = MainWindow::handleTimeshift();
   // } while(exitCode == 1);

    updatePackagesButton->setEnabled(false);
    refreshPackagesButton->setEnabled(false);
    selectProgramDropdown->setEnabled(false);

    consoleOutput->show();
    ui->packagesTable->hide();

    QStringList packagesToInstall;


    for(int i = 0; i < packages.size(); i++) {
        if(packages[i].state->isChecked())
            packagesToInstall.push_back(ui->packagesTable->item(i, 0)->text().trimmed());
    }

   if (process->isOpen())
        process->close();

    packagesToInstall.push_front("-S");

    process->setProgram("/usr/bin/"+selectProgramDropdown->currentText());
    process->setArguments(packagesToInstall);
    process->start();
}

void MainWindow::on_bulkSelectInputPressed(int checkedState)
{
    Qt::CheckState state = (checkedState == 0 ? Qt::CheckState::Unchecked : Qt::CheckState::Checked);

    for (int i = 0; i < ui->packagesTable->rowCount(); i++)
        packages[i].state->setCheckState(state);
}

bool MainWindow::hasTimeshift()
{
    if(!QFile::exists("/usr/share/libalpm/hooks/00-timeshift-autosnap.hook"))
        return false;

    return !system("which timeshift >> /dev/null 2>>/dev/null");
}

void MainWindow::checkDependencies()
{
    if ((system("which checkupdates && find /usr/share/libalpm/hooks/00-timeshift-autosnap.hook >> /dev/null 2>>/dev/null") != 0))
        MainWindow::showPopup(QMessageBox::Critical, "Dependency Missing", "checkupdates package not installed.");
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::resetTable()
{
    for(int i = 0; i < ui->packagesTable->rowCount(); i++)
        ui->packagesTable->removeRow(i);

    packages.clear();

    bulkSelectInput->setChecked(true);
}

void MainWindow::fetchUpdates()
{
    if(process->isOpen())
        process->close();

    QStringList emptyArgs;

    QString selectedProgram = selectProgramDropdown->currentText();

    if (selectedProgram == "yay") {
        process->setArguments({"-Qu"});
        process->setProgram("/usr/bin/yay");
    } else if (selectedProgram == "pacman") {
        process->setProgram("/usr/bin/checkupdates");
        process->setArguments(emptyArgs);
    }

    process->start();
}

void MainWindow::processCheckUpdates()
{
    QString output = process->readAllStandardOutput();
    QStringList lines = output.split("\n");

    const std::regex regex("(^[a-zA-Z0-9\\-\\.]+)\\s([\\+a-zA-Z0-9\\.\\-\\:]+)\\s->\\s([\\+A-Za-z0-9\\.\\-\\:]+)");

    std::smatch matches;

    if ( ui->packagesTable->columnCount() == 0) {
        ui->packagesTable->setColumnCount(4);
    }

    for (const auto & i: lines) {
        std::string line = i.toStdString();

        std::regex_match(line, matches, regex, std::regex_constants::match_not_null);

        if (!matches.size())
            continue;

        Package package;
        package.name = matches[1];
        package.current_version = matches[2];
        package.latest_version = matches[3];

        package.state = new QCheckBox;
        package.state->setCheckState(Qt::Checked);

        packages.push_back(package);

        ui->packagesTable->setRowCount(packages.size());

        QTableWidgetItem * item_name = new QTableWidgetItem(QString::fromStdString(package.name));
        QTableWidgetItem * current_version = new QTableWidgetItem(QString::fromStdString(package.current_version));
        QTableWidgetItem * latest_version = new QTableWidgetItem(QString::fromStdString(package.latest_version));

        int row = packages.size() - 1;

        ui->packagesTable->setItem(row, 0, item_name);
        ui->packagesTable->setItem(row, 1, current_version);
        ui->packagesTable->setItem(row, 2, latest_version);
        ui->packagesTable->setCellWidget(row, 3, package.state);

        connect(package.state, &QCheckBox::stateChanged, this, &MainWindow::on_PackageSelect);
    }

    ui->packagesTable->setRowCount(packages.size());
    selectedPackages = packages.size();
}

void MainWindow::on_PackageSelect(int state)
{
    if (state == Qt::Checked) {
        selectedPackages++;
    } else if (state == Qt::Unchecked) {
        selectedPackages--;
    }

    disconnect(bulkSelectInput, & QCheckBox::stateChanged, this, & MainWindow::on_bulkSelectInputPressed);

    if (selectedPackages < ui->packagesTable->rowCount()) {
        bulkSelectInput->setCheckState(Qt::Unchecked);
    } else if (selectedPackages == ui->packagesTable->rowCount()) {
        bulkSelectInput->setCheckState(Qt::Checked);
    }

    connect(bulkSelectInput, & QCheckBox::stateChanged, this, & MainWindow::on_bulkSelectInputPressed);

    MainWindow::updateRefreshButtonCount();
}

void MainWindow::updateRefreshButtonCount()
{
    QString ButtonLabel = "Update Packages (" + QString::number(selectedPackages) + ") ";
    updatePackagesButton->setText(ButtonLabel);
    updatePackagesButton->setEnabled((selectedPackages > 0));
}

void MainWindow::processOutput()
{
    QString output = process->readAllStandardOutput();

    consoleOutput->insertPlainText(output.toUtf8());

    if (output.startsWith("error", Qt::CaseInsensitive) && process->property("context").toString() != "cancelled")
        showPopup(QMessageBox::Critical, "Error", output, false);

    if (output.contains(":: Proceed with installation? [Y/n]")) {
        int response = showPopup(QMessageBox::Question, "Proceed with installation?", output);

        if (response == QMessageBox::Cancel || response == QMessageBox::No) {
            updatePackagesButton->setEnabled(true);
            refreshPackagesButton->setEnabled(true);
            selectProgramDropdown->setEnabled(true);
            consoleOutput->hide();
            ui->packagesTable->show();
            process->write("n");
            process->setProperty("context", "cancelled");
        }else if (response == QMessageBox::Yes) {
            process->write("y");
        }
        process->closeWriteChannel();
    } else if (output.contains("[y/N")) {
        int response = showPopup(QMessageBox::Question,output, output);
        response == (QMessageBox::Yes ? process->write("y") : process->write("n"));
        process->closeWriteChannel();
    }
}
void MainWindow::on_readyReadStandardOutput()
{
    QString currentProcess = process->program();

    bool isFetching = process->arguments().contains("-Qu");
    bool isUpdating = process->arguments().contains("-S");

    if (currentProcess == "/usr/bin/checkupdates" || currentProcess == "/usr/bin/yay" && isFetching) {
        checkboxes.clear();
        MainWindow::processCheckUpdates();
    } else if ((currentProcess == "/usr/bin/pacman" || currentProcess == "/usr/bin/yay") && isUpdating) {
        MainWindow::processOutput();
    }     
}
