// This file is part of RSS Guard.
//
// Copyright (C) 2011-2017 by Martin Rotter <rotter.martinos@gmail.com>
//
// RSS Guard is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// RSS Guard is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with RSS Guard. If not, see <http://www.gnu.org/licenses/>.

#ifndef FEEDSTOOLBAR_H
#define FEEDSTOOLBAR_H

#include "gui/basetoolbar.h"


class FeedsToolBar : public BaseToolBar {
    Q_OBJECT

  public:
    // Constructors and destructors.
    explicit FeedsToolBar(const QString &title, QWidget *parent = 0);
    virtual ~FeedsToolBar();

    QList<QAction*> availableActions() const;
    QList<QAction*> changeableActions() const;
    void saveChangeableActions(const QStringList &actions);

    QList<QAction*> getSpecificActions(const QStringList &actions);
    void loadSpecificActions(const QList<QAction*> &actions);

    QStringList defaultActions() const;
    QStringList savedActions() const;
};

#endif // FEEDSTOOLBAR_H
