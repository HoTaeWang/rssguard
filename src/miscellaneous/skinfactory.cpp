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

#include "miscellaneous/skinfactory.h"

#include "miscellaneous/application.h"

#include <QDir>
#include <QStyleFactory>
#include <QDomDocument>
#include <QDomElement>


SkinFactory::SkinFactory(QObject *parent) : QObject(parent) {
}

SkinFactory::~SkinFactory() {
}

void SkinFactory::loadCurrentSkin() {
  QList<QString> skin_names_to_try;

  skin_names_to_try.append(selectedSkinName());
  skin_names_to_try.append(APP_SKIN_DEFAULT);

  bool skin_parsed;
  Skin skin_data;
  QString skin_name;

  while (!skin_names_to_try.isEmpty()) {
    skin_name = skin_names_to_try.takeFirst();
    skin_data = skinInfo(skin_name, &skin_parsed);

    if (skin_parsed) {
      loadSkinFromData(skin_data);

      // Set this 'Skin' object as active one.
      m_currentSkin = skin_data;

      qDebug("Skin '%s' loaded.", qPrintable(skin_name));
      return;
    }
    else {
      qWarning("Failed to load skin '%s'.", qPrintable(skin_name));
    }
  }

  qCritical("Failed to load selected or default skin. Quitting!");
}

void SkinFactory::loadSkinFromData(const Skin &skin) {  
  if (!skin.m_rawData.isEmpty()) {
    qApp->setStyleSheet(skin.m_rawData);
  }

  qApp->setStyle(qApp->settings()->value(GROUP(GUI), SETTING(GUI::Style)).toString());
}

void SkinFactory::setCurrentSkinName(const QString &skin_name) {
  qApp->settings()->setValue(GROUP(GUI), GUI::Skin, skin_name);
}

QString SkinFactory::getUserSkinBaseFolder() const {
  return qApp->getUserDataPath() + QDir::separator() + APP_SKIN_USER_FOLDER;
}

QString SkinFactory::selectedSkinName() const {
  return qApp->settings()->value(GROUP(GUI), SETTING(GUI::Skin)).toString();
}

Skin SkinFactory::skinInfo(const QString &skin_name, bool *ok) const {
  Skin skin;
  QStringList base_skin_folders;

  base_skin_folders.append(APP_SKIN_PATH);
  base_skin_folders.append(getUserSkinBaseFolder());

  while (!base_skin_folders.isEmpty()) {
    const QString skin_folder = base_skin_folders.takeAt(0) + QDir::separator() + skin_name + QDir::separator();
    const QString metadata_file = skin_folder + APP_SKIN_METADATA_FILE;

    if (QFile::exists(metadata_file)) {
      QFile skin_file(metadata_file);

      QDomDocument dokument;

      if (!skin_file.open(QIODevice::Text | QIODevice::ReadOnly) || !dokument.setContent(&skin_file, true)) {
        if (ok) {
          *ok = false;
        }

        return skin;
      }

      const QDomNode skin_node = dokument.namedItem(QSL("skin"));

      // Obtain visible skin name.
      skin.m_visibleName = skin_name;

      // Obtain author.
      skin.m_author = skin_node.namedItem(QSL("author")).namedItem(QSL("name")).toElement().text();

      // Obtain email.
      skin.m_email = skin_node.namedItem(QSL("author")).namedItem(QSL("email")).toElement().text();

      // Obtain version.
      skin.m_version = skin_node.attributes().namedItem(QSL("version")).toAttr().value();

      // Obtain other information.
      skin.m_baseName = skin_name;

      // Free resources.
      skin_file.close();
      skin_file.deleteLater();

      // Here we use "/" instead of QDir::separator() because CSS2.1 url field
      // accepts '/' as path elements separator.
      //
      // "##" is placeholder for the actual path to skin file. This is needed for using
      // images within the QSS file.
      // So if one uses "##/images/border.png" in QSS then it is
      // replaced by fully absolute path and target file can
      // be safely loaded.

      skin.m_layoutMarkupWrapper = QString::fromUtf8(IOFactory::readTextFile(skin_folder + QL1S("html_wrapper.html")));
      skin.m_layoutMarkupWrapper = skin.m_layoutMarkupWrapper.replace(QSL("##"), APP_SKIN_PATH + QL1S("/") + skin_name);

      skin.m_enclosureImageMarkup = QString::fromUtf8(IOFactory::readTextFile(skin_folder + QL1S("html_enclosure_image.html")));
      skin.m_enclosureImageMarkup = skin.m_enclosureImageMarkup.replace(QSL("##"), APP_SKIN_PATH + QL1S("/") + skin_name);

      skin.m_layoutMarkup = QString::fromUtf8(IOFactory::readTextFile(skin_folder + QL1S("html_single_message.html")));
      skin.m_layoutMarkup = skin.m_layoutMarkup.replace(QSL("##"), APP_SKIN_PATH + QL1S("/") + skin_name);

      skin.m_enclosureMarkup = QString::fromUtf8(IOFactory::readTextFile(skin_folder + QL1S("html_enclosure_every.html")));
      skin.m_enclosureMarkup = skin.m_enclosureMarkup.replace(QSL("##"), APP_SKIN_PATH + QL1S("/") + skin_name);

      skin.m_rawData = QString::fromUtf8(IOFactory::readTextFile(skin_folder + QL1S("theme.css")));
      skin.m_rawData = skin.m_rawData.replace(QSL("##"), APP_SKIN_PATH + QL1S("/") + skin_name);

      if (ok != nullptr) {
        *ok = !skin.m_author.isEmpty() && !skin.m_version.isEmpty() &&
              !skin.m_baseName.isEmpty() && !skin.m_email.isEmpty() &&
              !skin.m_layoutMarkup.isEmpty();
      }

      break;
    }
  }

  return skin;
}

QList<Skin> SkinFactory::installedSkins() const {
  QList<Skin> skins;
  bool skin_load_ok;
  QStringList skin_directories = QDir(APP_SKIN_PATH).entryList(QDir::Dirs |
                                                               QDir::NoDotAndDotDot |
                                                               QDir::NoSymLinks |
                                                               QDir::Readable);
  skin_directories.append(QDir(getUserSkinBaseFolder()).entryList(QDir::Dirs |
                                                                  QDir::NoDotAndDotDot |
                                                                  QDir::NoSymLinks |
                                                                  QDir::Readable));

  foreach (const QString &base_directory, skin_directories) {
    const Skin skin_info = skinInfo(base_directory, &skin_load_ok);

    if (skin_load_ok) {
      skins.append(skin_info);
    }
  }

  return skins;
}
