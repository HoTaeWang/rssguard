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

#ifndef FEEDSVIEW_H
#define FEEDSVIEW_H

#include <QTreeView>

#include "core/feedsmodel.h"

#include <QStyledItemDelegate>


class FeedsProxyModel;
class Feed;
class Category;

class FeedsView : public QTreeView {
    Q_OBJECT

  public:
    // Constructors and destructors.
    explicit FeedsView(QWidget *parent = 0);
    virtual ~FeedsView();

    // Fundamental accessors.
    inline FeedsProxyModel *model() const {
      return m_proxyModel;
    }

    inline FeedsModel *sourceModel() const {
      return m_sourceModel;
    }

    void setSortingEnabled(bool enable);
    
    // Returns list of selected/all feeds.
    // NOTE: This is recursive method which returns all descendants.
    QList<Feed*> selectedFeeds() const;

    // Returns pointers to selected feed/category if they are really
    // selected.
    RootItem *selectedItem() const;

    // Saves/loads expand states of all nodes (feeds/categories) of the list to/from settings.
    void saveAllExpandStates();
    void loadAllExpandStates();

  public slots:   
    void sortByColumn(int column, Qt::SortOrder order);

    void addFeedIntoSelectedAccount();
    void addCategoryIntoSelectedAccount();
    void expandCollapseCurrentItem();

    // Feed updating.
    void updateSelectedItems();

    // Feed read/unread manipulators.
    void markSelectedItemRead();
    void markSelectedItemUnread();
    void markAllItemsRead();

    // Newspaper accessors.
    void openSelectedItemsInNewspaperMode();

    // Feed clearers.
    void clearSelectedFeeds();
    void clearAllFeeds();

    // Base manipulators.
    void editSelectedItem();
    void deleteSelectedItem();

    // Selects next/previous item (feed/category) in the list.
    void selectNextItem();
    void selectPreviousItem();

    // Switches visibility of the widget.
    void switchVisibility();

  signals:
    // Emitted if user selects new feeds.
    void itemSelected(RootItem *item);

    // Requests opening of given messages in newspaper mode.
    void openMessagesInNewspaperView(RootItem *root, const QList<Message> &messages);

  protected:
    // Handle selections.
    void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected);

    // React on "Del" key.
    void keyPressEvent(QKeyEvent *event);

    // Show custom context menu.
    void contextMenuEvent(QContextMenuEvent *event);

    void mouseDoubleClickEvent(QMouseEvent *event);

  private slots:
    void expandItemDelayed(const QModelIndex &idx);
    void markSelectedItemReadStatus(RootItem::ReadStatus read);
    void markAllItemsReadStatus(RootItem::ReadStatus read);

    void saveSortState(int column, Qt::SortOrder order);
    void validateItemAfterDragDrop(const QModelIndex &source_index);
    void onItemExpandRequested(const QList<RootItem*> &items, bool exp);
    void onItemExpandStateSaveRequested(RootItem *item);

  private:
    // Initializes context menus.
    QMenu *initializeContextMenuCategories(RootItem *clicked_item);
    QMenu *initializeContextMenuFeeds(RootItem *clicked_item);
    QMenu *initializeContextMenuEmptySpace();
    QMenu *initializeContextMenuOtherItem(RootItem *clicked_item);

    // Sets up appearance of this widget.
    void setupAppearance();

    void saveExpandStates(RootItem *item);

    QMenu *m_contextMenuCategories;
    QMenu *m_contextMenuFeeds;
    QMenu *m_contextMenuEmptySpace;
    QMenu *m_contextMenuOtherItems;

    FeedsModel *m_sourceModel;
    FeedsProxyModel *m_proxyModel;
};

#endif // FEEDSVIEW_H
