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
#include "macrotable.h"
#include "exception.h"

#include <QStringList>
#include <QRegExp>
#include <QDebug>
#include <windows.h>

namespace NMakeFile {

MacroTable::MacroTable()
{
}

MacroTable::~MacroTable()
{
}

QString MacroTable::macroValue(const QString& macroName) const
{
    return m_macros.value(macroName).value;
}

/**
 * Sets the value of a macro and marks it as environment variable.
 * That means changing the macro value changes the environment.
 * Note that environment macro names are converted to upper case.
 */
void MacroTable::defineEnvironmentMacroValue(const QString& name, const QString& value, bool readOnly)
{
    if (m_macros.contains(name))
        return;
    MacroData* macroData = internalSetMacroValue(name.toUpper(), value);
    if (!macroData)
        return;
    macroData->isEnvironmentVariable = true;
    macroData->isReadOnly = readOnly;
    setEnvironmentVariable(name, value);
}

bool MacroTable::isMacroNameValid(const QString& name) const
{
    static QRegExp rexMacroIdentifier;
    if (rexMacroIdentifier.isEmpty()) {
        rexMacroIdentifier.setPattern("([A-Z]|_|)(\\w|\\.)+");
        rexMacroIdentifier.setCaseSensitivity(Qt::CaseInsensitive);
    }

    return rexMacroIdentifier.exactMatch(name);
}

/**
 * Sets the value of a macro. If the macro doesn't exist, it is defines as
 * a normal macro (no environment variable) - changing the macro doesn't affect
 * the environment.
 * If the macros exists and is an environment variable then the corresponding
 * environment variable is set to the new macro value.
 */
void MacroTable::setMacroValue(const QString& name, const QString& value)
{
    MacroData* macroData = internalSetMacroValue(name, value);
    if (!macroData)
        throw Exception(QString("macro name %1 is invalid").arg(name));

    if (macroData->isEnvironmentVariable)
        setEnvironmentVariable(name, value);
}

/**
 * Sets the value of an environment variable.
 * The environment will be passed to the QProcess instances.
 */
void MacroTable::setEnvironmentVariable(const QString& name, const QString& value)
{
    //### Changing the actual environment can be removed when we don't call system() anymore.
    ::SetEnvironmentVariableW(reinterpret_cast<const WCHAR*>(name.utf16()),
                              reinterpret_cast<const WCHAR*>(value.utf16()));

    const QString namePlusEq = name + "=";
    QStringList::iterator it = m_environment.begin();
    QStringList::iterator itEnd = m_environment.end();
    for (; it != itEnd; ++it) {
        if ((*it).startsWith(namePlusEq, Qt::CaseInsensitive)) {
            m_environment.erase(it);
            break;
        }
    }
    m_environment.append(namePlusEq + value);
}

MacroTable::MacroData* MacroTable::internalSetMacroValue(const QString& name, const QString& value)
{
    QString expandedName = expandMacros(name);
    if (!isMacroNameValid(expandedName))
        return 0;

    MacroData* result = 0;
    const QString instantiatedName = "$(" + expandedName + ")";
    QString newValue = value;
    if (value.contains(instantiatedName))
        newValue.replace(instantiatedName, macroValue(expandedName));

    result = &m_macros[expandedName];
    if (!result->isReadOnly)
        result->value = newValue;

    return result;
}

bool MacroTable::isMacroDefined(const QString& name) const
{
    return m_macros.contains(name);
}

void MacroTable::undefineMacro(const QString& name)
{
    m_macros.remove(name);
}

QString MacroTable::expandMacros(const QString& str) const
{
    QSet<QString> usedMacros;
    return expandMacros(str, usedMacros);
}

QString MacroTable::expandMacros(const QString& str, QSet<QString>& usedMacros) const
{
    QString ret;
    ret.reserve(str.count());

    int i = 0;
    const int max_i = str.count() - 2;
    while (i < str.count()) {
        if (i >= max_i) {
            ret.append(str.mid(i));
            break;
        }
        if (str.at(i) == QLatin1Char('$')) {
            ++i;
            if (str.at(i) == QLatin1Char('(')) {
                // found standard macro invokation a la $(MAKE)
                int k = str.indexOf(QLatin1Char(')'), i);
                if (k < 0)
                    throw Exception("Macro invokation $( without closing ) found");

                const QString macroName = str.mid(i + 1, k - i - 1);
                if (macroName.isEmpty())
                    throw Exception("Macro name is missing from invokation");

                switch (macroName.at(0).toLatin1())
                {
                case '@':
                case '*':
                    {
                        ret.append(QLatin1String("$("));
                        ret.append(macroName);
                        ret.append(QLatin1String(")"));
                    }
                    break;
                default:
                    {
                        QString macroValue = cycleCheckedMacroValue(macroName, usedMacros);
                        macroValue = expandMacros(macroValue, usedMacros);
                        usedMacros.remove(macroName);
                        ret.append(macroValue);
                    }
                }
                i = k;
            } else if (str.at(i) == QLatin1Char('$')) {
                // found escaped $ char
                ret.append(QLatin1Char('$'));
            } else if (str.at(i).isLetterOrNumber()) {
                // found single character macro invokation a la $X
                const QString macroName = str.at(i);
                QString macroValue = cycleCheckedMacroValue(macroName, usedMacros);
                macroValue = expandMacros(macroValue, usedMacros);
                usedMacros.remove(macroName);
                ret.append(macroValue);
            } else {
                switch (str.at(i).toLatin1())
                {
                case '<':
                case '*':
                case '@':
                case '?':
                    ret.append(QLatin1Char('$'));
                    ret.append(str.at(i));
                    break;
                default:
                    throw Exception("Invalid macro invokation found");
                }
            }
        } else {
            ret.append(str.at(i));
        }
        ++i;
    }

    ret.squeeze();
    return ret;
}

QString MacroTable::cycleCheckedMacroValue(const QString& macroName, QSet<QString>& usedMacros) const
{
    if (usedMacros.contains(macroName)) {
        QString msg = QLatin1String("Cycle in macro detected when trying to invoke $(%1).");
        throw Exception(msg.arg(macroName));
    }
    usedMacros.insert(macroName);
    return macroValue(macroName);
}

void MacroTable::dump() const
{
    QHash<QString, MacroData>::const_iterator it = m_macros.begin();
    for (; it != m_macros.end(); ++it) {
        printf(qPrintable(it.key()));
        printf(" = ");
        printf(qPrintable((*it).value));
        printf("\n");
    }
}

} // namespace NMakeFile
