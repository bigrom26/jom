/****************************************************************************
 **
 ** Copyright (C) 2008-2012 Nokia Corporation and/or its subsidiary(-ies).
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
#pragma once

#include <QtCore/QStringList>

namespace NMakeFile {

class Makefile;
class Options;

class MakefileFactory
{
public:
    MakefileFactory();
    void setEnvironment(const QStringList& env) { m_environment = env; }
    bool apply(const QStringList& commandLineArguments, Options **outopt = 0);

    enum ErrorType {
        NoError,
        CommandLineError,
        ParserError,
        IOError
    };

    Makefile* makefile() { return m_makefile; }
    const QStringList& activeTargets() const { return m_activeTargets; }
    const QString& errorString() const { return m_errorString; }
    ErrorType errorType() const { return m_errorType; }

private:
    void clear();

private:
    Makefile*   m_makefile;
    QStringList m_environment;
    QStringList m_activeTargets;
    QString     m_errorString;
    ErrorType   m_errorType;
};

} // namespace NMakeFile
