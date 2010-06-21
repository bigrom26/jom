/****************************************************************************
 **
 ** Copyright (C) 2008-2010 Nokia Corporation and/or its subsidiary(-ies).
 ** Contact: Nokia Corporation (qt-info@nokia.com)
 **
 ** This file is part of the jom project on Trolltech Labs.
 **
 ** This file may be used under the terms of the GNU General Public
 ** License version 2.0 or 3.0 as published by the Free Software Foundation
 ** and appearing in the file LICENSE.GPL included in the packaging of
 ** this file.  Please review the following information to ensure GNU
 ** General Public Licensing requirements will be met:
 ** http://www.fsf.org/licensing/licenses/info/GPLv2.html and
 ** http://www.gnu.org/copyleft/gpl.html.
 **
 ** If you are unsure which license is appropriate for your use, please
 ** contact the sales department at qt-sales@nokia.com.
 **
 ** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 ** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 **
 ****************************************************************************/
#include <options.h>
#include <parser.h>
#include <preprocessor.h>
#include <targetexecutor.h>
#include <exception.h>
#include <makefilefactory.h>

#include <QDebug>
#include <QDir>
#include <QTextStream>
#include <QCoreApplication>
#include <QProcess>
#include <QTextCodec>

#include <windows.h>
#include <Tlhelp32.h>

using namespace NMakeFile;

const int nVersionMajor = 0;
const int nVersionMinor = 9;
const int nVersionPatch = 3;

static void showLogo()
{
    fprintf(stderr, "\njom %d.%d.%d - experimental version\n\n",
        nVersionMajor, nVersionMinor, nVersionPatch);
}

static void showUsage()
{
    printf("Usage: jom @commandfile\n"
           "       jom [options] [/f makefile] [macro definitions] [targets]\n\n"
           "This tool is meant to be an nmake clone.\n"
           "Please see the Microsoft nmake documentation for more options.\n"
           "/DUMPGRAPH show the generated dependency graph\n"
           "/DUMPGRAPHDOT dump dependency graph in dot format\n");
}

static TargetExecutor* g_pTargetExecutor = 0;

BOOL WINAPI ConsoleCtrlHandlerRoutine(__in  DWORD /*dwCtrlType*/)
{
    fprintf(stderr, "jom terminated by user (pid=%u)\n", QCoreApplication::applicationPid());

    HANDLE hSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapShot == INVALID_HANDLE_VALUE)
        return 0;
    BOOL bSuccess;
    const DWORD dwThisPID = QCoreApplication::applicationPid();
    PROCESSENTRY32 processEntry;
    processEntry.dwSize = sizeof(processEntry);
    bSuccess = Process32First(hSnapShot, &processEntry);
    while (bSuccess) {
        if (processEntry.th32ParentProcessID == dwThisPID) {
            HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, TRUE, processEntry.th32ProcessID);
            if (hProcess != INVALID_HANDLE_VALUE) {
                //fprintf(stderr, "terminating process %u\n", processEntry.th32ProcessID);
                TerminateProcess(hProcess, 5);
                CloseHandle(hProcess);
            }
        }
        bSuccess = Process32Next(hSnapShot, &processEntry);
    }
    CloseHandle(hSnapShot);

    if (g_pTargetExecutor)
        g_pTargetExecutor->removeTempFiles();

    exit(2);
    return TRUE;
}

int main(int argc, char* argv[])
{
    SetConsoleCtrlHandler(&ConsoleCtrlHandlerRoutine, TRUE);
    QCoreApplication app(argc, argv);
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("IBM 850"));

    QStringList systemEnvironment = QProcess::systemEnvironment();
    MakefileFactory mf;
    mf.setEnvironment(systemEnvironment);
    if (!mf.apply(qApp->arguments().mid(1))) {
        switch (mf.errorType()) {
        case MakefileFactory::CommandLineError:
            showUsage();
            return 128;
        case MakefileFactory::ParserError:
            fprintf(stderr, "ERROR: ");
            fprintf(stderr, qPrintable(mf.errorString()));
            fprintf(stderr, "\n");
            return 2;
        }
    }

    Makefile* mkfile = mf.makefile();
    const Options* options = mkfile->options();

    if (options->showUsageAndExit) {
        if (options->showLogo)
            showLogo();
        showUsage();
        return 0;
    } else if (options->showVersionAndExit) {
        printf("jom version %d.%d.%d\n", nVersionMajor, nVersionMinor, nVersionPatch);
        return 0;
    }

    if (options->showLogo)
        showLogo();

    if (options->displayMakeInformation) {
        printf("MACROS:\n\n");
        mkfile->macroTable()->dump();
        printf("\nINFERENCE RULES:\n\n");
        mkfile->dumpInferenceRules();
        printf("\nTARGETS:\n\n");
        mkfile->dumpTargets();
    }

    TargetExecutor executor(systemEnvironment);
    g_pTargetExecutor = &executor;
    try {
        executor.apply(mkfile, mf.activeTargets());
    }
    catch (Exception e) {
        QString msg = "Error in executor: " + e.message() + "\n";
        fprintf(stderr, qPrintable(msg));
    }

    app.postEvent(&executor, new TargetExecutor::StartEvent());
    int result = app.exec();
    g_pTargetExecutor = 0;
    delete mkfile;
    return result;
}
