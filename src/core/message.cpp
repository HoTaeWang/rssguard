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

#include "core/message.h"

#include "miscellaneous/textfactory.h"

#include <QVariant>


Enclosure::Enclosure(const QString &url, const QString &mime) : m_url(url), m_mimeType(mime) {
}

QList<Enclosure> Enclosures::decodeEnclosuresFromString(const QString &enclosures_data) {
  QList<Enclosure> enclosures;

  foreach (const QString &single_enclosure, enclosures_data.split(ENCLOSURES_OUTER_SEPARATOR, QString::SkipEmptyParts)) {
    Enclosure enclosure;

    if (single_enclosure.contains(ECNLOSURES_INNER_SEPARATOR)) {
      QStringList mime_url = single_enclosure.split(ECNLOSURES_INNER_SEPARATOR);

      enclosure.m_mimeType = QByteArray::fromBase64(mime_url.at(0).toLocal8Bit());
      enclosure.m_url = QByteArray::fromBase64(mime_url.at(1).toLocal8Bit());
    }
    else {
      enclosure.m_url = QByteArray::fromBase64(single_enclosure.toLocal8Bit());
    }

    enclosures.append(enclosure);
  }

  return enclosures;
}

QString Enclosures::encodeEnclosuresToString(const QList<Enclosure> &enclosures) {
  QStringList enclosures_str;

  foreach (const Enclosure &enclosure, enclosures) {
    if (enclosure.m_mimeType.isEmpty()) {
      enclosures_str.append(enclosure.m_url.toLocal8Bit().toBase64());
    }
    else {
      enclosures_str.append(QString(enclosure.m_mimeType.toLocal8Bit().toBase64()) +
                            ECNLOSURES_INNER_SEPARATOR +
                            enclosure.m_url.toLocal8Bit().toBase64());
    }
  }

  return enclosures_str.join(QString(ENCLOSURES_OUTER_SEPARATOR));
}

Message::Message() {
  m_title = m_url = m_author = m_contents = m_feedId = m_customId = m_customHash = QSL("");
  m_enclosures = QList<Enclosure>();
  m_accountId = m_id = 0;
  m_isRead = m_isImportant = false;
}

Message Message::fromSqlRecord(const QSqlRecord &record, bool *result) {
  if (record.count() != MSG_DB_CUSTOM_HASH_INDEX + 1) {
    if (result != nullptr) {
      *result = false;
      return Message();
    }
  }

  Message message;

  message.m_id = record.value(MSG_DB_ID_INDEX).toInt();
  message.m_isRead = record.value(MSG_DB_READ_INDEX).toBool();
  message.m_isImportant = record.value(MSG_DB_IMPORTANT_INDEX).toBool();
  message.m_feedId = record.value(MSG_DB_FEED_CUSTOM_ID_INDEX).toString();
  message.m_title = record.value(MSG_DB_TITLE_INDEX).toString();
  message.m_url = record.value(MSG_DB_URL_INDEX).toString();
  message.m_author = record.value(MSG_DB_AUTHOR_INDEX).toString();
  message.m_created = TextFactory::parseDateTime(record.value(MSG_DB_DCREATED_INDEX).value<qint64>());
  message.m_contents = record.value(MSG_DB_CONTENTS_INDEX).toString();
  message.m_enclosures = Enclosures::decodeEnclosuresFromString(record.value(MSG_DB_ENCLOSURES_INDEX).toString());
  message.m_accountId = record.value(MSG_DB_ACCOUNT_ID_INDEX).toInt();
  message.m_customId = record.value(MSG_DB_CUSTOM_ID_INDEX).toString();
  message.m_customHash = record.value(MSG_DB_CUSTOM_HASH_INDEX).toString();

  if (result != nullptr) {
    *result = true;
  }

  return message;
}

uint qHash(Message key, uint seed) {
  Q_UNUSED(seed)
  return (key.m_accountId * 10000) + key.m_id;
}

uint qHash(const Message &key) {
  return (key.m_accountId * 10000) + key.m_id;
}
