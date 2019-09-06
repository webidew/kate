/*  This file is part of the Kate project.
 *
 *  Copyright (C) 2017 Héctor Mesa Jiménez <hector@lcc.uma.es>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#ifndef KATE_PROJECT_CODE_ANALYSIS_TOOL_FLAKE8_H
#define KATE_PROJECT_CODE_ANALYSIS_TOOL_FLAKE8_H

#include "../kateprojectcodeanalysistool.h"

/**
 * Information provider for flake8
 */
class KateProjectCodeAnalysisToolFlake8: public KateProjectCodeAnalysisTool
{
public:
    explicit KateProjectCodeAnalysisToolFlake8(QObject *parent = nullptr);

    ~KateProjectCodeAnalysisToolFlake8() override;

    QString name() const override;

    QString description() const override;

    QString fileExtensions() const override;

    virtual QStringList filter(const QStringList &files) const override;

    QString path() const override;

    QStringList arguments() override;

    QString notInstalledMessage() const override;

    QStringList parseLine(const QString &line) const override;

    QString stdinMessages() override;
};

#endif // KATE_PROJECT_CODE_ANALYSIS_TOOL_FLAKE8_H
