/****************************************************************************
 **
 ** Copyright (C) 2008-2011 Nokia Corporation and/or its subsidiary(-ies).
 ** Contact: Nokia Corporation (info@qt.nokia.com)
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
#include "application.h"
#include <options.h>
#include <parser.h>
#include <preprocessor.h>
#include <targetexecutor.h>
#include <exception.h>
#include <makefilefactory.h>

#include <QDebug>
#include <QDir>
#include <QTextStream>
#include <QProcess>
#include <QTextCodec>

#include <windows.h>
#include <Tlhelp32.h>

#ifndef __in
#define __in
#endif

using namespace NMakeFile;

const int nVersionMajor = 1;
const int nVersionMinor = 0;
const int nVersionPatch = 7;

static void showLogo()
{
    fprintf(stderr, "\njom %d.%d.%d - empower your cores\n\n",
        nVersionMajor, nVersionMinor, nVersionPatch);
}

static void showUsage()
{
    printf("Usage: jom @commandfile\n"
           "       jom [options] [/f makefile] [macro definitions] [targets]\n\n"
           "This tool is meant to be an nmake clone.\n"
           "Please see the Microsoft nmake documentation for more options.\n"
           "/DUMPGRAPH show the generated dependency graph\n"
           "/DUMPGRAPHDOT dump dependency graph in dot format\n"
           "/KEEPTEMPFILES keeps all temporary files\n");
}

static TargetExecutor* g_pTargetExecutor = 0;

BOOL WINAPI ConsoleCtrlHandlerRoutine(__in  DWORD /*dwCtrlType*/)
{
    fprintf(stderr, "jom terminated by user (pid=%u)\n", QCoreApplication::applicationPid());

    GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0);
    if (g_pTargetExecutor)
        g_pTargetExecutor->removeTempFiles();

    exit(2);
    return TRUE;
}

QStringList getCommandLineArguments()
{
    QStringList commandLineArguments = qApp->arguments().mid(1);
    QByteArray makeFlags = qgetenv("MAKEFLAGS");
    if (!makeFlags.isEmpty())
        commandLineArguments.prepend(QString::fromLatin1('/' + makeFlags));
    return commandLineArguments;
}

int main(int argc, char* argv[])
{
    SetConsoleCtrlHandler(&ConsoleCtrlHandlerRoutine, TRUE);
    Application app(argc, argv);
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("IBM 850"));

    MakefileFactory mf;
    mf.setEnvironment(QProcess::systemEnvironment());
    if (!mf.apply(getCommandLineArguments())) {
        switch (mf.errorType()) {
        case MakefileFactory::CommandLineError:
            showUsage();
            return 128;
        case MakefileFactory::ParserError:
        case MakefileFactory::IOError:
            fprintf(stderr, "Error: ");
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

    TargetExecutor executor(mkfile->macroTable()->environment());
    g_pTargetExecutor = &executor;
    try {
        executor.apply(mkfile, mf.activeTargets());
    }
    catch (Exception &e) {
        fprintf(stderr, "Error in executor: %s\n", qPrintable(e.message()));
    }

    QMetaObject::invokeMethod(&executor, "startProcesses", Qt::QueuedConnection);
    int result = app.exec();
    g_pTargetExecutor = 0;
    delete mkfile;
    return result;
}
