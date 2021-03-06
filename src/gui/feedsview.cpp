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

#include "gui/feedsview.h"

#include "definitions/definitions.h"
#include "core/feedsmodel.h"
#include "core/feedsproxymodel.h"
#include "services/abstract/rootitem.h"
#include "miscellaneous/systemfactory.h"
#include "miscellaneous/feedreader.h"
#include "miscellaneous/mutex.h"
#include "gui/systemtrayicon.h"
#include "gui/messagebox.h"
#include "gui/styleditemdelegatewithoutfocus.h"
#include "gui/dialogs/formmain.h"
#include "services/abstract/feed.h"
#include "services/abstract/serviceroot.h"
#include "services/standard/standardcategory.h"
#include "services/standard/standardfeed.h"
#include "services/standard/gui/formstandardcategorydetails.h"

#include <QMenu>
#include <QHeaderView>
#include <QContextMenuEvent>
#include <QPointer>
#include <QPainter>
#include <QTimer>


FeedsView::FeedsView(QWidget *parent)
  : QTreeView(parent),
    m_contextMenuCategories(nullptr),
    m_contextMenuFeeds(nullptr),
    m_contextMenuEmptySpace(nullptr),
    m_contextMenuOtherItems(nullptr) {
  setObjectName(QSL("FeedsView"));

  // Allocate models.
  m_sourceModel = qApp->feedReader()->feedsModel();
  m_proxyModel = qApp->feedReader()->feedsProxyModel();

  // Connections.
  connect(m_sourceModel, &FeedsModel::requireItemValidationAfterDragDrop, this, &FeedsView::validateItemAfterDragDrop);
  connect(m_sourceModel, &FeedsModel::itemExpandRequested, this, &FeedsView::onItemExpandRequested);
  connect(m_sourceModel, &FeedsModel::itemExpandStateSaveRequested, this, &FeedsView::onItemExpandStateSaveRequested);
  connect(header(), &QHeaderView::sortIndicatorChanged, this, &FeedsView::saveSortState);
  connect(m_proxyModel, &FeedsProxyModel::expandAfterFilterIn, this, &FeedsView::expandItemDelayed);

  setModel(m_proxyModel);
  setupAppearance();
}

FeedsView::~FeedsView() {
  qDebug("Destroying FeedsView instance.");
}

void FeedsView::setSortingEnabled(bool enable) {
  disconnect(header(), &QHeaderView::sortIndicatorChanged, this, &FeedsView::saveSortState);
  QTreeView::setSortingEnabled(enable);
  connect(header(), &QHeaderView::sortIndicatorChanged, this, &FeedsView::saveSortState);
}

QList<Feed*> FeedsView::selectedFeeds() const {
  const QModelIndex current_index = currentIndex();

  if (current_index.isValid()) {
    return m_sourceModel->feedsForIndex(m_proxyModel->mapToSource(current_index));
  }
  else {
    return QList<Feed*>();
  }
}

RootItem *FeedsView::selectedItem() const {
  const QModelIndexList selected_rows = selectionModel()->selectedRows();

  if (selected_rows.isEmpty()) {
    return nullptr;
  }
  else {
    RootItem *selected_item = m_sourceModel->itemForIndex(m_proxyModel->mapToSource(selected_rows.at(0)));
    return selected_item == m_sourceModel->rootItem() ? nullptr : selected_item;
  }
}

void FeedsView::onItemExpandStateSaveRequested(RootItem *item) {
  saveExpandStates(item);
}

void FeedsView::saveAllExpandStates() {
  saveExpandStates(sourceModel()->rootItem());
}

void FeedsView::saveExpandStates(RootItem *item) {
  Settings *settings = qApp->settings();
  QList<RootItem*> items = item->getSubTree(RootItemKind::Category | RootItemKind::ServiceRoot);

  // Iterate all categories and save their expand statuses.
  foreach (const RootItem *item, items) {
    const QString setting_name = item->hashCode();

    QModelIndex source_index = sourceModel()->indexForItem(item);
    QModelIndex visible_index = model()->mapFromSource(source_index);

    settings->setValue(GROUP(CategoriesExpandStates),
                       setting_name,
                       isExpanded(visible_index));
  }
}

void FeedsView::loadAllExpandStates() {
  const Settings *settings = qApp->settings();
  QList<RootItem*> expandable_items;

  expandable_items.append(sourceModel()->rootItem()->getSubTree(RootItemKind::Category | RootItemKind::ServiceRoot));

  // Iterate all categories and save their expand statuses.
  foreach (const RootItem *item, expandable_items) {
    const QString setting_name = item->hashCode();

    setExpanded(model()->mapFromSource(sourceModel()->indexForItem(item)),
                settings->value(GROUP(CategoriesExpandStates), setting_name, item->childCount() > 0).toBool());
  }

  sortByColumn(qApp->settings()->value(GROUP(GUI), SETTING(GUI::DefaultSortColumnFeeds)).toInt(),
               static_cast<Qt::SortOrder>(qApp->settings()->value(GROUP(GUI), SETTING(GUI::DefaultSortOrderFeeds)).toInt()));
}

void FeedsView::sortByColumn(int column, Qt::SortOrder order) {
  const int old_column = header()->sortIndicatorSection();
  const Qt::SortOrder old_order = header()->sortIndicatorOrder();

  if (column == old_column && order == old_order) {
    m_proxyModel->sort(column, order);
  }
  else {
    QTreeView::sortByColumn(column, order);
  }
}

void FeedsView::addFeedIntoSelectedAccount() {
  const RootItem *selected = selectedItem();

  if (selected != nullptr) {
    ServiceRoot *root = selected->getParentServiceRoot();

    if (root->supportsFeedAdding()) {
      root->addNewFeed();
    }
    else {
      qApp->showGuiMessage(tr("Not supported"),
                           tr("Selected account does not support adding of new feeds."),
                           QSystemTrayIcon::Warning,
                           qApp->mainFormWidget(), true);
    }
  }
}

void FeedsView::addCategoryIntoSelectedAccount() {
  const RootItem *selected = selectedItem();

  if (selected != nullptr) {
    ServiceRoot *root = selected->getParentServiceRoot();

    if (root->supportsCategoryAdding()) {
      root->addNewCategory();
    }
    else {
      qApp->showGuiMessage(tr("Not supported"),
                           tr("Selected account does not support adding of new categories."),
                           QSystemTrayIcon::Warning,
                           qApp->mainFormWidget(), true);
    }
  }
}

void FeedsView::expandCollapseCurrentItem() {
  if (selectionModel()->selectedRows().size() == 1) {
    QModelIndex index = selectionModel()->selectedRows().at(0);

    if (!index.child(0, 0).isValid() && index.parent().isValid()) {
      setCurrentIndex(index.parent());
      index = index.parent();
    }

    isExpanded(index) ? collapse(index) : expand(index);
  }
}

void FeedsView::updateSelectedItems() {
  qApp->feedReader()->updateFeeds(selectedFeeds());
}

void FeedsView::clearSelectedFeeds() {
  m_sourceModel->markItemCleared(selectedItem(), false);
}

void FeedsView::clearAllFeeds() {
  m_sourceModel->markItemCleared(m_sourceModel->rootItem(), false);
}

void FeedsView::editSelectedItem() {
  if (!qApp->feedUpdateLock()->tryLock()) {
    // Lock was not obtained because
    // it is used probably by feed updater or application
    // is quitting.
    qApp->showGuiMessage(tr("Cannot edit item"),
                         tr("Selected item cannot be edited because another critical operation is ongoing."),
                         QSystemTrayIcon::Warning, qApp->mainFormWidget(), true);
    // Thus, cannot delete and quit the method.
    return;
  }

  if (selectedItem()->canBeEdited()) {
    selectedItem()->editViaGui();
  }
  else {
    qApp->showGuiMessage(tr("Cannot edit item"),
                         tr("Selected item cannot be edited, this is not (yet?) supported."),
                         QSystemTrayIcon::Warning,
                         qApp->mainFormWidget(),
                         true);
  }

  // Changes are done, unlock the update master lock.
  qApp->feedUpdateLock()->unlock();
}

void FeedsView::deleteSelectedItem() {
  if (!qApp->feedUpdateLock()->tryLock()) {
    // Lock was not obtained because
    // it is used probably by feed updater or application
    // is quitting.
    qApp->showGuiMessage(tr("Cannot delete item"),
                         tr("Selected item cannot be deleted because another critical operation is ongoing."),
                         QSystemTrayIcon::Warning, qApp->mainFormWidget(), true);

    // Thus, cannot delete and quit the method.
    return;
  }

  if (!currentIndex().isValid()) {
    // Changes are done, unlock the update master lock and exit.
    qApp->feedUpdateLock()->unlock();
    return;
  }

  RootItem *selected_item = selectedItem();

  if (selected_item != nullptr) {
    if (selected_item->canBeDeleted()) {
      // Ask user first.
      if (MessageBox::show(qApp->mainFormWidget(),
                           QMessageBox::Question,
                           tr("Deleting \"%1\"").arg(selected_item->title()),
                           tr("You are about to completely delete item \"%1\".").arg(selected_item->title()),
                           tr("Are you sure?"),
                           QString(), QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::No) {
        // User refused.
        qApp->feedUpdateLock()->unlock();
        return;
      }

      // We have deleteable item selected, remove it via GUI.
      if (!selected_item->deleteViaGui()) {
        qApp->showGuiMessage(tr("Cannot delete \"%1\"").arg(selected_item->title()),
                             tr("This item cannot be deleted because something critically failed. Submit bug report."),
                             QSystemTrayIcon::Critical,
                             qApp->mainFormWidget(),
                             true);
      }
    }
    else {
      qApp->showGuiMessage(tr("Cannot delete \"%1\"").arg(selected_item->title()),
                           tr("This item cannot be deleted, because it does not support it\nor this functionality is not implemented yet."),
                           QSystemTrayIcon::Critical,
                           qApp->mainFormWidget(),
                           true);
    }
  }

  // Changes are done, unlock the update master lock.
  qApp->feedUpdateLock()->unlock();
}

void FeedsView::markSelectedItemReadStatus(RootItem::ReadStatus read) {
  m_sourceModel->markItemRead(selectedItem(), read);
}

void FeedsView::markSelectedItemRead() {
  markSelectedItemReadStatus(RootItem::Read);
}

void FeedsView::markSelectedItemUnread() {
  markSelectedItemReadStatus(RootItem::Unread);
}

void FeedsView::markAllItemsReadStatus(RootItem::ReadStatus read) {
  m_sourceModel->markItemRead(m_sourceModel->rootItem(), read);
}

void FeedsView::markAllItemsRead() {
  markAllItemsReadStatus(RootItem::Read);
}

void FeedsView::openSelectedItemsInNewspaperMode() {
  RootItem *selected_item = selectedItem();
  const QList<Message> messages = m_sourceModel->messagesForItem(selected_item);

  if (!messages.isEmpty()) {
    emit openMessagesInNewspaperView(selected_item, messages);
  }
}

void FeedsView::selectNextItem() {
  const QModelIndex index_next = moveCursor(QAbstractItemView::MoveDown, Qt::NoModifier);

  if (index_next.isValid()) {
    setCurrentIndex(index_next);
    setFocus();
  }
}

void FeedsView::selectPreviousItem() {
  const QModelIndex index_previous = moveCursor(QAbstractItemView::MoveUp, Qt::NoModifier);

  if (index_previous.isValid()) {
    setCurrentIndex(index_previous);
    setFocus();
  }
}

void FeedsView::switchVisibility() {
  setVisible(!isVisible());
}

void FeedsView::expandItemDelayed(const QModelIndex &idx) {
  QTimer::singleShot(100, this, [=] {
    // TODO: Z nastavení.
    setExpanded(m_proxyModel->mapFromSource(idx), true);
  });
}

QMenu *FeedsView::initializeContextMenuCategories(RootItem *clicked_item) {
  if (m_contextMenuCategories == nullptr) {
    m_contextMenuCategories = new QMenu(tr("Context menu for categories"), this);
  }
  else {
    m_contextMenuCategories->clear();
  }

  QList<QAction*> specific_actions = clicked_item->contextMenu();

  m_contextMenuCategories->addActions(QList<QAction*>() <<
                                      qApp->mainForm()->m_ui->m_actionUpdateSelectedItems <<
                                      qApp->mainForm()->m_ui->m_actionEditSelectedItem <<
                                      qApp->mainForm()->m_ui->m_actionViewSelectedItemsNewspaperMode <<
                                      qApp->mainForm()->m_ui->m_actionMarkSelectedItemsAsRead <<
                                      qApp->mainForm()->m_ui->m_actionMarkSelectedItemsAsUnread <<
                                      qApp->mainForm()->m_ui->m_actionDeleteSelectedItem);

  if (!specific_actions.isEmpty()) {
    m_contextMenuCategories->addSeparator();
    m_contextMenuCategories->addActions(specific_actions);
  }

  return m_contextMenuCategories;
}

QMenu *FeedsView::initializeContextMenuFeeds(RootItem *clicked_item) {
  if (m_contextMenuFeeds == nullptr) {
    m_contextMenuFeeds = new QMenu(tr("Context menu for categories"), this);
  }
  else {
    m_contextMenuFeeds->clear();
  }

  QList<QAction*> specific_actions = clicked_item->contextMenu();

  m_contextMenuFeeds->addActions(QList<QAction*>() <<
                                 qApp->mainForm()->m_ui->m_actionUpdateSelectedItems <<
                                 qApp->mainForm()->m_ui->m_actionEditSelectedItem <<
                                 qApp->mainForm()->m_ui->m_actionViewSelectedItemsNewspaperMode <<
                                 qApp->mainForm()->m_ui->m_actionMarkSelectedItemsAsRead <<
                                 qApp->mainForm()->m_ui->m_actionMarkSelectedItemsAsUnread <<
                                 qApp->mainForm()->m_ui->m_actionDeleteSelectedItem);

  if (!specific_actions.isEmpty()) {
    m_contextMenuFeeds->addSeparator();
    m_contextMenuFeeds->addActions(specific_actions);
  }

  return m_contextMenuFeeds;
}

QMenu *FeedsView::initializeContextMenuEmptySpace() {
  if (m_contextMenuEmptySpace == nullptr) {
    m_contextMenuEmptySpace = new QMenu(tr("Context menu for empty space"), this);
    m_contextMenuEmptySpace->addAction(qApp->mainForm()->m_ui->m_actionUpdateAllItems);
    m_contextMenuEmptySpace->addSeparator();
  }

  return m_contextMenuEmptySpace;
}

QMenu *FeedsView::initializeContextMenuOtherItem(RootItem *clicked_item) {
  if (m_contextMenuOtherItems == nullptr) {
    m_contextMenuOtherItems = new QMenu(tr("Context menu for other items"), this);
  }
  else {
    m_contextMenuOtherItems->clear();
  }

  QList<QAction*> specific_actions = clicked_item->contextMenu();

  if (!specific_actions.isEmpty()) {
    m_contextMenuOtherItems->addSeparator();
    m_contextMenuOtherItems->addActions(specific_actions);
  }
  else {
    m_contextMenuOtherItems->addAction(qApp->mainForm()->m_ui->m_actionNoActions);
  }

  return m_contextMenuOtherItems;
}

void FeedsView::setupAppearance() {
  // Setup column resize strategies.
  header()->setSectionResizeMode(FDS_MODEL_TITLE_INDEX, QHeaderView::Stretch);
  header()->setSectionResizeMode(FDS_MODEL_COUNTS_INDEX, QHeaderView::ResizeToContents);

  setUniformRowHeights(true);
  setAnimated(true);
  setSortingEnabled(true);
  setItemsExpandable(true);
  setExpandsOnDoubleClick(true);
  setEditTriggers(QAbstractItemView::NoEditTriggers);
  setIndentation(FEEDS_VIEW_INDENTATION);
  setAcceptDrops(false);
  setDragEnabled(true);
  setDropIndicatorShown(true);
  setDragDropMode(QAbstractItemView::InternalMove);
  setAllColumnsShowFocus(false);
  setRootIsDecorated(false);
  setSelectionMode(QAbstractItemView::SingleSelection);
  setItemDelegate(new StyledItemDelegateWithoutFocus(this));
  header()->setStretchLastSection(false);
  header()->setSortIndicatorShown(false);
}

void FeedsView::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected) {
  RootItem *selected_item = selectedItem();

  m_proxyModel->setSelectedItem(selected_item);
  QTreeView::selectionChanged(selected, deselected);
  emit itemSelected(selected_item);
  m_proxyModel->invalidateReadFeedsFilter();
}

void FeedsView::keyPressEvent(QKeyEvent *event) {
  QTreeView::keyPressEvent(event);

  if (event->key() == Qt::Key_Delete) {
    deleteSelectedItem();
  }
}

void FeedsView::contextMenuEvent(QContextMenuEvent *event) {
  const QModelIndex clicked_index = indexAt(event->pos());

  if (clicked_index.isValid()) {
    const QModelIndex mapped_index = model()->mapToSource(clicked_index);
    RootItem *clicked_item = sourceModel()->itemForIndex(mapped_index);

    if (clicked_item->kind() == RootItemKind::Category) {
      // Display context menu for categories.
      initializeContextMenuCategories(clicked_item)->exec(event->globalPos());
    }
    else if (clicked_item->kind() == RootItemKind::Feed) {
      // Display context menu for feeds.
      initializeContextMenuFeeds(clicked_item)->exec(event->globalPos());
    }
    else {
      initializeContextMenuOtherItem(clicked_item)->exec(event->globalPos());
    }
  }
  else {
    // Display menu for empty space.
    initializeContextMenuEmptySpace()->exec(event->globalPos());
  }
}

void FeedsView::mouseDoubleClickEvent(QMouseEvent *event) {
  QModelIndex idx = indexAt(event->pos());

  if (idx.isValid()) {
    RootItem *item = m_sourceModel->itemForIndex(m_proxyModel->mapToSource(idx));

    if (item->kind() == RootItemKind::Feed || item->kind() == RootItemKind::Bin) {
      const QList<Message> messages = m_sourceModel->messagesForItem(item);

      if (!messages.isEmpty()) {
        emit openMessagesInNewspaperView(item, messages);
      }
    }
  }

  QTreeView::mouseDoubleClickEvent(event);
}

void FeedsView::saveSortState(int column, Qt::SortOrder order) {
  qApp->settings()->setValue(GROUP(GUI), GUI::DefaultSortColumnFeeds, column);
  qApp->settings()->setValue(GROUP(GUI), GUI::DefaultSortOrderFeeds, order);
}

void FeedsView::validateItemAfterDragDrop(const QModelIndex &source_index) {
  const QModelIndex mapped = m_proxyModel->mapFromSource(source_index);

  if (mapped.isValid()) {
    expand(mapped);
    setCurrentIndex(mapped);
  }
}

void FeedsView::onItemExpandRequested(const QList<RootItem*> &items, bool exp) {
  foreach (const RootItem *item, items) {
    QModelIndex source_index = m_sourceModel->indexForItem(item);
    QModelIndex proxy_index = m_proxyModel->mapFromSource(source_index);

    //setExpanded(proxy_index, !exp);
    setExpanded(proxy_index, exp);
  }
}
