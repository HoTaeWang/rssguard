// This file is part of RSS Guard.
//
// Copyright (C) 2011-2015 by Martin Rotter <rotter.martinos@gmail.com>
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

#include "services/abstract/serviceroot.h"

#include "core/feedsmodel.h"
#include "miscellaneous/application.h"
#include "miscellaneous/textfactory.h"
#include "services/abstract/category.h"

#include <QSqlQuery>


ServiceRoot::ServiceRoot(RootItem *parent) : RootItem(parent), m_accountId(NO_PARENT_CATEGORY) {
  setKind(RootItemKind::ServiceRoot);
}

ServiceRoot::~ServiceRoot() {
}

bool ServiceRoot::deleteViaGui() {
  QSqlDatabase connection = qApp->database()->connection(metaObject()->className(), DatabaseFactory::FromSettings);

  // Remove all messages.
  if (!QSqlQuery(connection).exec(QString("DELETE FROM Messages WHERE account_id = %1;").arg(accountId()))) {
    return false;
  }

  // Remove all feeds.
  if (!QSqlQuery(connection).exec(QString("DELETE FROM Feeds WHERE account_id = %1;").arg(accountId()))) {
    return false;
  }

  // Remove all categories.
  if (!QSqlQuery(connection).exec(QString("DELETE FROM Categories WHERE account_id = %1;").arg(accountId()))) {
    return false;
  }

  // Switch "existence" flag.
  bool data_removed = QSqlQuery(connection).exec(QString("DELETE FROM Accounts WHERE id = %1;").arg(accountId()));

  if (data_removed) {
    requestItemRemoval(this);
  }

  return data_removed;
}

bool ServiceRoot::markAsReadUnread(RootItem::ReadStatus status) {
  QSqlDatabase db_handle = qApp->database()->connection(metaObject()->className(), DatabaseFactory::FromSettings);

  if (!db_handle.transaction()) {
    qWarning("Starting transaction for feeds read change.");
    return false;
  }

  QSqlQuery query_read_msg(db_handle);
  query_read_msg.setForwardOnly(true);
  query_read_msg.prepare(QSL("UPDATE Messages SET is_read = :read WHERE is_pdeleted = 0 AND account_id = :account_id;"));

  query_read_msg.bindValue(QSL(":account_id"), accountId());
  query_read_msg.bindValue(QSL(":read"), status == RootItem::Read ? 1 : 0);

  if (!query_read_msg.exec()) {
    qDebug("Query execution for feeds read change failed.");
    db_handle.rollback();
  }

  // Commit changes.
  if (db_handle.commit()) {
    updateCounts(false);
    itemChanged(getSubTree());
    requestReloadMessageList(status == RootItem::Read);
    return true;
  }
  else {
    return db_handle.rollback();
  }
}

QList<Message> ServiceRoot::undeletedMessages() const {
  QList<Message> messages;
  int account_id = accountId();
  QSqlDatabase database = qApp->database()->connection(metaObject()->className(), DatabaseFactory::FromSettings);
  QSqlQuery query_read_msg(database);

  query_read_msg.setForwardOnly(true);
  query_read_msg.prepare("SELECT * "
                         "FROM Messages "
                         "WHERE is_deleted = 0 AND is_pdeleted = 0 AND account_id = :account_id;");
  query_read_msg.bindValue(QSL(":account_id"), account_id);

  // FIXME: Fix those const functions, this is fucking ugly.

  if (query_read_msg.exec()) {
    while (query_read_msg.next()) {
      bool decoded;
      Message message = Message::fromSqlRecord(query_read_msg.record(), &decoded);

      if (decoded) {
        messages.append(message);
      }

      messages.append(message);
    }
  }

  return messages;
}

void ServiceRoot::itemChanged(const QList<RootItem *> &items) {
  emit dataChanged(items);
}

void ServiceRoot::requestReloadMessageList(bool mark_selected_messages_read) {
  emit reloadMessageListRequested(mark_selected_messages_read);
}

void ServiceRoot::requestFeedReadFilterReload() {
  emit readFeedsFilterInvalidationRequested();
}

void ServiceRoot::requestItemExpand(const QList<RootItem *> &items, bool expand) {
  emit itemExpandRequested(items, expand);
}

void ServiceRoot::requestItemReassignment(RootItem *item, RootItem *new_parent) {
  emit itemReassignmentRequested(item, new_parent);
}

void ServiceRoot::requestItemRemoval(RootItem *item) {
  emit itemRemovalRequested(item);
}

int ServiceRoot::accountId() const {
  return m_accountId;
}

void ServiceRoot::setAccountId(int account_id) {
  m_accountId = account_id;
}

void ServiceRoot::assembleFeeds(Assignment feeds) {
  QHash<int,Category*> categories = getHashedSubTreeCategories();

  foreach (const AssignmentItem &feed, feeds) {
    if (feed.first == NO_PARENT_CATEGORY) {
      // This is top-level feed, add it to the root item.
      appendChild(feed.second);
      feed.second->updateCounts(true);
    }
    else if (categories.contains(feed.first)) {
      // This feed belongs to this category.
      categories.value(feed.first)->appendChild(feed.second);
      feed.second->updateCounts(true);
    }
    else {
      qWarning("Feed '%s' is loose, skipping it.", qPrintable(feed.second->title()));
    }
  }
}

void ServiceRoot::assembleCategories(Assignment categories) {
  QHash<int,RootItem*> assignments;
  assignments.insert(NO_PARENT_CATEGORY, this);

  // Add top-level categories.
  while (!categories.isEmpty()) {
    for (int i = 0; i < categories.size(); i++) {
      if (assignments.contains(categories.at(i).first)) {
        // Parent category of this category is already added.
        assignments.value(categories.at(i).first)->appendChild(categories.at(i).second);

        // Now, added category can be parent for another categories, add it.
        assignments.insert(categories.at(i).second->id(), categories.at(i).second);

        // Remove the category from the list, because it was
        // added to the final collection.
        categories.removeAt(i);
        i--;
      }
    }
  }
}